#include "Engine.h"
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

using namespace physics;

// constants
const auto Engine::kGravity = Eigen::Vector3d(0, -9.8, 0);



/*
 * Initializes the physics engine and its worker thread.
 */
Engine::Engine(std::shared_ptr<render::SceneRenderer> &_scene, render::Camera *_cam) : 
    scene(_scene), camera(_cam) {
    this->run = true;
    this->thread = std::make_unique<std::thread>(&Engine::main, this);

    this->tickHandler = world::TickHandler::add(std::bind(&Engine::notifyTick, this));
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
}

/**
 * Sends a tick event to the worker thread.
 */
void Engine::notifyTick() {
    this->physEvents.enqueue(TickEvent());
}



/**
 * Sets the player position.
 */
void Engine::setPlayerPosition(const glm::vec3 &newPos) {
    this->playerPosition = newPos;
}

/**
 * Apply some vertical velocity to allow the player to jump upwards.
 */
void Engine::playerJump() {
    if(!this->playerFalling) {
        this->physEvents.enqueue(PlayerVelocity(Eigen::Vector3d(0, kJumpVelocity, 0)));
        Logging::trace("jumpendir");

        /// XXX: hax alert
        this->playerFalling = true;
    }
}



/**
 * Main loop for the worker thread
 */
void Engine::main() {
    // initialize engine
    this->lastPhysStep = std::chrono::high_resolution_clock::now();

    // run loop
    while(this->run) {
        // get event
        Event e;
        this->physEvents.wait_dequeue(e);

        if(std::holds_alternative<std::monostate>(e)) continue;

        // process the event type
        if(std::holds_alternative<TickEvent>(e)) {
            this->step();
        } else if(std::holds_alternative<PlayerVelocity>(e)) {
            const auto &v = std::get<PlayerVelocity>(e);

            this->playerVelocity += v.velocity;
            Logging::trace("Applying impulse: {} {} {}", this->playerVelocity(0), this->playerVelocity(1), this->playerVelocity(2));
        }
    }

    Logging::debug("Physics engine worker exiting");
}

/**
 * Performs one step of the physics simulation.
 */
void Engine::step() {
    using namespace std::chrono;
    PROFILE_SCOPE(PhysicsStep);

    // determine if we should be falling or not
    this->checkPlayerFalling();

    // calculate integration constant
    const auto now = high_resolution_clock::now();
    const auto diffUs = std::chrono::duration_cast<std::chrono::microseconds>(now - this->lastPhysStep).count();
    const float h = std::min(((float) diffUs) / 1000. / 1000., .05);

    // perform the various steps
    this->updatePlayerVelocities(h);

    // record tick time
    this->lastPhysStep = now;
}

/**
 * Checks whether we're falling by checking if there's a solid block underneath us or not.
 *
 * Note the use of ceil() rather than floor(); this means that if we're even slightly above y=0
 * offset of a block, we'll get pulled back down.
 */
void Engine::checkPlayerFalling() {
    PROFILE_SCOPE(CheckPlayerFalling);

    // get block position and the associated chunk
    const glm::vec3 blockPos(floor(this->playerPosition.x), ceil(this->playerPosition.y + kYBias - 1),
            floor(this->playerPosition.z));

    glm::ivec2 chunkPos(0);
    glm::ivec3 blockOff(0);
    world::Chunk::absoluteToRelative(blockPos, chunkPos, blockOff);

    // grab the block below us
    auto chunk = this->scene->getChunk(chunkPos);
    if(!chunk) {
        Logging::trace("Not falling because chunk not found: {}", chunkPos);
        this->playerFalling = false;
        return;
    }

    auto block = chunk->getBlock(blockOff);
    if(!block) {
        // if a block isn't found, assume it's air and we can be falling through it
        this->playerFalling = true;
        return;
    }

    // we're falling depends on whether that block is collidable or not
    this->playerFalling = !world::BlockRegistry::isCollidableBlock(*block);
}

/**
 * Updates the player's Y position.
 */
void Engine::updatePlayerVelocities(const float h) {
    // get velocities
    auto velocity = this->playerVelocity;

    // apply gravity; if not falling, we just apply an equal opposite force
    velocity = h * kPlayerMass * kGravity;

    if(!this->playerFalling) {
    //    velocity = h * kPlayerMass * -kGravity;
        velocity(1) = std::max(0., velocity(1));
    }

    // integrate new position and apply to player
    this->playerVelocity = velocity;
    const auto delta = h * velocity;
    const auto yDelta = delta(1);

    Logging::trace("Player velocity: {}, pos delta: {} falling {}", velocity(1), delta(1), this->playerFalling);

    if(yDelta) {
        world::TickHandler::defer([&, yDelta] {
            this->camera->applyRawDeltas(glm::vec3(0, yDelta, 0));
        });
    }
}
