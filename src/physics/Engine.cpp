#include "Engine.h"
#include "PlayerWorldCollisionHandler.h"

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
#include <Eigen/Eigen>

// physics library has a few warnings that need to be fixed but we'll just suppress them for now
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#include <reactphysics3d/reactphysics3d.h> 
#pragma GCC diagnostic pop

using namespace physics;

// constants
const auto Engine::kGravity = Eigen::Vector3d(0, -9.8, 0);



/*
 * Initializes the physics engine and its worker thread.
 */
Engine::Engine(std::shared_ptr<render::SceneRenderer> &_scene, render::Camera *_cam) : 
    scene(_scene), camera(_cam) {
    // this->run = true;
    // this->thread = std::make_unique<std::thread>(&Engine::main, this);

    this->playerCollision = new PlayerWorldCollisionHandler(_scene);

    this->tickHandler = world::TickHandler::add(std::bind(&Engine::notifyTick, this));
    this->lastPlayerStep = std::chrono::high_resolution_clock::now();

    // create the physics engine
    this->common = new reactphysics3d::PhysicsCommon;
    this->world = this->common->createPhysicsWorld();
}

/**
 * Shuts down the physics engine worker thread.
 */
Engine::~Engine() {
    // remove tick handler
    world::TickHandler::remove(this->tickHandler);

    // clear flag and signal before waiting for thread to join
    this->run = false;
    this->physEvents.enqueue(std::monostate());

    this->thread->join();

    delete this->playerCollision;

    // all physics resources will automatically be deallocated from this :D
    delete this->common;
}

/**
 * Sends a tick event to the worker thread.
 */
void Engine::notifyTick() {
    this->physEvents.enqueue(std::monostate());
}



/**
 * Main loop for the worker thread
 */
void Engine::main() {
    // run loop
    while(this->run) {
        // get event
        Event e;
        this->physEvents.wait_dequeue(e);

        if(std::holds_alternative<std::monostate>(e)) continue;
    }

    Logging::debug("Physics engine worker exiting");
}



/**
 * Special step for handling the player movement. This is called on each frame from the rendering
 * loop; it does simple intersection checks of the proposed position.
 */
void Engine::movePlayer(const glm::vec3 &deltas, const bool jump) {
    // then, check to see whether the calculated next position is ok (collision wise)
    const auto nextPos = this->camera->deltasToPos(deltas);
    if(this->playerCollision->isPositionOk(nextPos)) {
        this->camera->updatePosition(deltas);
    }
}
