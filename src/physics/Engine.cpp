#include "Engine.h"
#include "Types.h"

#include "util/Intersect.h"
#include "render/Camera.h"
#include "render/scene/SceneRenderer.h"
#include "world/chunk/Chunk.h"
#include "world/block/BlockRegistry.h"
#include "world/tick/TickHandler.h"
#include "io/Format.h"

#include <Logging.h>
#include <mutils/time/profiler.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <metricsgui/metrics_gui.h>

// physics library has a few warnings that need to be fixed but we'll just suppress them for now
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#include <reactphysics3d/reactphysics3d.h> 
#pragma GCC diagnostic pop

using namespace physics;



/*
 * Initializes the physics engine and its worker thread.
 */
Engine::Engine(std::shared_ptr<render::SceneRenderer> &_scene, render::Camera *_cam) : 
    scene(_scene), camera(_cam) {
    using namespace reactphysics3d;

    // create the react3d physics engine
    this->common = new PhysicsCommon;
    this->world = this->common->createPhysicsWorld();

    // create the player body
    Transform transform(Vector3(), Quaternion::identity());
    this->playerBody = this->world->createRigidBody(transform);

    this->playerBody->setMass(kPlayerMass);
    this->playerBody->enableGravity(false);

    auto shapnes = this->common->createBoxShape(Vector3(1, kPlayerHeight, 1));
    Transform shapnesT(Vector3(0, -kPlayerHeight/2., 0), Quaternion::identity());
    this->playerCollider = this->playerBody->addCollider(shapnes, shapnesT);

    // then set up the UI for metrics
    this->mAccumulator = new MetricsGuiMetric("Accumulator", "s", MetricsGuiMetric::USE_SI_UNIT_PREFIX);
    this->mStepTime = new MetricsGuiMetric("Step Time", "s", MetricsGuiMetric::USE_SI_UNIT_PREFIX);

    this->mPlot = new MetricsGuiPlot;
    this->mPlot->mInlinePlotRowCount = 3;
    this->mPlot->mShowInlineGraphs = true;
    this->mPlot->mShowAverage = true;
    this->mPlot->mShowLegendUnits = false;

    this->mPlot->AddMetric(this->mAccumulator);
    this->mPlot->AddMetric(this->mStepTime);
}

/**
 * Shuts down the physics engine worker thread.
 */
Engine::~Engine() {
    // all physics resources will automatically be deallocated from this :D
    delete this->common;

    // get rid of UI stuff
    delete this->mPlot;
    delete this->mAccumulator;
    delete this->mStepTime;
}


/**
 * Sets the player position. A new translation for the player to bring it to the given position
 * (and angles) is created.
 */
void Engine::setPlayerPosition(const glm::vec3 &pos, const glm::vec3 &angles) {
    using namespace reactphysics3d;

    Transform transform(vec(pos), Quaternion::identity());

    this->lastPlayerTransform = transform;
    this->playerBody->setTransform(transform);
}

/**
 * Sends any movement deltas (as well as jumping) to the physics engine. This in turn manifests
 * itself as the application of some forces to the player body.
 */
void Engine::movePlayer(const glm::vec3 &deltas, const bool jump) {
    // const auto nextPos = this->camera->deltasToPos(deltas);
    const auto dir = this->camera->deltasToDirVec(deltas);

    // create force vector
    const auto force = dir * glm::vec3(kMovementForce, 0, kMovementForce);

    if(!glm::any(glm::isnan(force))) {
        this->playerBody->applyForceToCenterOfMass(vec(force));
        Logging::trace("Forces: {}", force);
    }

    // Logging::debug("Deltas {} -> position {} direction {}", deltas, nextPos, dir);
    // this->camera->updatePosition(deltas);
}



/**
 * Called at the start of a frame to step the physics simulation.
 *
 * This employs a relatively simple scheme whereby we "accumulate" time on each frame, and the
 * physics simulation is run with fixed time steps until the accumulator is drained.
 */
void Engine::startFrame() {
    PROFILE_SCOPE(Physics);
    using namespace std::chrono;
    using namespace reactphysics3d;

    // get current time, calculate deltas and add to accumulator. bail if first time through
    const auto now = high_resolution_clock::now();
    if(this->numSteps++ == 0) {
        this->lastFrameTime = now;
        return;
    }

    const auto diff = now - this->lastFrameTime;
    const auto deltaTime = ((float) duration_cast<microseconds>(diff).count())/1000./1000.;

    this->lastFrameTime = now;
    this->stepAccumulator += deltaTime;

    // perform physics steps as long as there are steps in the accumulator
    while(this->stepAccumulator >= kTimeStep) {
        PROFILE_SCOPE(Step);

        const auto stepStart = high_resolution_clock::now();
        this->world->update(kTimeStep);
        const auto stepDiff = high_resolution_clock::now() - stepStart;
        const auto stepSecs = ((float) duration_cast<microseconds>(stepDiff).count())/1000./ 1000.;
        this->mStepTime->AddNewValue(stepSecs);

        this->stepAccumulator -= kTimeStep;
    }

    this->mAccumulator->AddNewValue(this->stepAccumulator);

    // update transform of the player
    const auto playerTransform = this->playerBody->getTransform();

    if(this->numSteps > 1) {
        // we have a previous frame's transform so interpolate
        const float factor = this->stepAccumulator / kTimeStep;
        const auto interp = Transform::interpolateTransforms(this->lastPlayerTransform,
                playerTransform, factor);

        // update camera with this transform
        const glm::vec3 newPos = vec(interp.getPosition());
        this->camera->setCameraPosition(newPos);
    }

    this->lastPlayerTransform = playerTransform;

    // show debug UI if needed
    if(this->showDebugWindow) {
        this->drawDebugUi();
    }
}



/**
 * Draws the physics engine debugger view.
 */
void Engine::drawDebugUi() {
    PROFILE_SCOPE(DebugUi);

    if(!ImGui::Begin("Physics Engine", &this->showDebugWindow)) {
        return;
    }

    // player translation
    const auto pos = vec(this->playerBody->getTransform().getPosition());
    ImGui::Text("Player Translation: (%g, %g, %g)", pos.x, pos.y, pos.z);

    // metrics
    this->mPlot->UpdateAxes();

    if(ImGui::CollapsingHeader("Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
        this->mPlot->DrawList();
    }

    // end drawing
    ImGui::End();
}
