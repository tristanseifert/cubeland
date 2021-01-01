#include "Engine.h"
#include "Types.h"
#include "EngineDebugRenderer.h"
#include "BlockCollision.h"

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
    this->playerBody->setAngularDamping(0.74);
    this->playerBody->setLinearDamping(kPlayerLinearDamping);
    this->playerBody->enableGravity(false);
    this->playerBody->setAngularVelocityFactor(Vector3(0, 1, 0));

    auto shapnes = this->common->createBoxShape(Vector3(.45, kPlayerHeight/2., .45));
    Transform shapnesT(Vector3(.45, kPlayerHeight/2., .45), Quaternion::identity());
    this->playerCollider = this->playerBody->addCollider(shapnes, shapnesT);

    auto &playerMat = this->playerCollider->getMaterial();
    playerMat.setBounciness(kPlayerBounciness);
    playerMat.setFrictionCoefficient(kPlayerFriction);

    // various other components
    this->blockCol = new BlockCollision(this);

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
    // various components of the engine
    delete this->blockCol;

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
    // create movement force vector
    const auto dir = this->camera->deltasToDirVec(deltas);
    auto force = dir * glm::vec3(kMovementForce, 0, kMovementForce);

    if(glm::any(glm::isnan(force))) {
        force = glm::vec3(0);
    }

    // apply force for jumping if needed
    if(jump && !this->jump) {
        force += glm::vec3(0, kJumpForce, 0);
        this->jump = true;
    } else if(!jump) {
        this->jump = false;
    }

    // apply force
    this->playerBody->applyForceToCenterOfMass(vec(force));

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

    if(this->dbgUpdateNeeded) {
        this->updateDebugFlags();
    }

    this->blockCol->startFrame();

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
    {
        LOCK_GUARD(this->engineLock, EngineLock);
        while(this->stepAccumulator >= kTimeStep) {
            PROFILE_SCOPE(Step);

            const auto stepStart = high_resolution_clock::now();

            this->singleStep();

            const auto stepDiff = high_resolution_clock::now() - stepStart;
            const auto stepSecs = ((float) duration_cast<microseconds>(stepDiff).count())/1000./ 1000.;
            this->mStepTime->AddNewValue(stepSecs);

            this->stepAccumulator -= kTimeStep;
        }
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
 * Performs a single step of the simulation.
 */
void Engine::singleStep() {
    // set gravity effects on player if the chunk we're under is loaded
    glm::ivec2 currentChunk;

    const glm::ivec3 newPos = floor(vec(this->playerBody->getTransform().getPosition()));
    world::Chunk::absoluteToRelative(newPos, currentChunk);
    const bool hasChunk = (this->scene->getChunk(currentChunk) != nullptr);

    this->playerBody->enableGravity(hasChunk);

    // perform stepping
    this->blockCol->update();
    this->world->update(kTimeStep);
}

/**
 * Sets physics engine debugging flags.
 */
void Engine::updateDebugFlags() {
    using Item = reactphysics3d::DebugRenderer::DebugItem;

    // clear update flags
    this->dbgUpdateNeeded = false;

    // set the physics world's debug flags
    this->world->setIsDebugRenderingEnabled(this->dbgDrawInfo);
    if(this->dbgStep) {
        this->dbgStep->setDrawsDebugData(this->dbgDrawInfo);
    }

    if(!this->dbgDrawInfo) return;

    // toggle the enabled views
    auto &dr = this->world->getDebugRenderer();

    dr.setIsDebugItemDisplayed(Item::COLLIDER_AABB, this->dbgDrawColliderAabb);
    dr.setIsDebugItemDisplayed(Item::COLLIDER_BROADPHASE_AABB, this->dbgDrawColliderBroadphase);
    dr.setIsDebugItemDisplayed(Item::COLLISION_SHAPE, this->dbgDrawCollisionShape);
    dr.setIsDebugItemDisplayed(Item::CONTACT_POINT, this->dbgDrawContactPoints);
    dr.setIsDebugItemDisplayed(Item::CONTACT_NORMAL, this->dbgDrawContactNormals);
}

/**
 * Sets the renderer step to display physics engine data.
 */
void Engine::setDebugRenderStep(std::shared_ptr<EngineDebugRenderer> &dbg) {
    dbg->world = this->world;

    this->dbgStep = dbg;
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

    // block offsets
    if(ImGui::DragFloat3("Block Offset", &this->blockCol->blockTranslate.x, 0.001, -1, 1)) {
        this->blockCol->removeAllBlocks();
    }

    // debugging drawing options
    if(ImGui::Checkbox("Draw Debugging Info", &this->dbgDrawInfo)) {
        this->dbgUpdateNeeded = true;
    }

    ImGui::Indent();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 255, 255));
    if(ImGui::Checkbox("Collider AABBs", &this->dbgDrawColliderAabb)) {
        this->dbgUpdateNeeded = true;
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
    if(ImGui::Checkbox("Collider Broadphase AABBs", &this->dbgDrawColliderBroadphase)) {
        this->dbgUpdateNeeded = true;
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
    if(ImGui::Checkbox("Collision Shapes", &this->dbgDrawCollisionShape)) {
        this->dbgUpdateNeeded = true;
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    if(ImGui::Checkbox("Contact Points", &this->dbgDrawContactPoints)) {
        this->dbgUpdateNeeded = true;
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
    if(ImGui::Checkbox("Contact Normals", &this->dbgDrawContactNormals)) {
        this->dbgUpdateNeeded = true;
    }
    ImGui::PopStyleColor();

    // end drawing
    ImGui::End();
}
