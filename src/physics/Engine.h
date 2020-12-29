/**
 * Implements the game's physics engine.
 */
#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include <memory>
#include <atomic>
#include <thread>
#include <variant>
#include <chrono>

#include <glm/vec3.hpp>
#include <Eigen/Eigen>
#include <blockingconcurrentqueue.h>

namespace render {
class SceneRenderer;
class Camera;
}

namespace physics {
class Engine {
    public:
        Engine(std::shared_ptr<render::SceneRenderer> &scene, render::Camera *cam);
        ~Engine();

        void playerJump();
        void setPlayerPosition(const glm::vec3 &newPos);

    private:
        void notifyTick();

        void main();
        void step();

        void checkPlayerFalling();
        void updatePlayerVelocities(const float h);

    private:
        /// bias value added to the current Y position when checking floating state
        constexpr static const double kYBias = -0.2;

        /// Gravity vector (defined in m/s)
        static const Eigen::Vector3d kGravity;
        /// Mass of the player (in kg)
        constexpr static const double kPlayerMass = 5;
        /// Velocity to apply when jumping
        constexpr static const double kJumpVelocity = 2.;

    private:
        /// Applies a velocity to the player
        struct PlayerVelocity {
            PlayerVelocity(const Eigen::Vector3d &_v) : velocity(_v) {}

            Eigen::Vector3d velocity;
        };

        /// Indicates a tick occurred
        struct TickEvent {
            uint8_t dummy = 0;
        };

        using Event = std::variant<std::monostate, TickEvent, PlayerVelocity>;

    private:
        /// get chunk data from this scene
        std::shared_ptr<render::SceneRenderer> scene = nullptr;
        /// camera for viewing
        render::Camera *camera = nullptr;

        /// flag to keep the engine running
        std::atomic_bool run;
        /// physics engine thread
        std::unique_ptr<std::thread> thread;
        /// events sent to the worker thread
        moodycamel::BlockingConcurrentQueue<Event> physEvents;

        /// id of the tick handler. this forwards the event to the physics engine
        uint32_t tickHandler = 0;

        /// time at which the last physics step took place
        std::chrono::high_resolution_clock::time_point lastPhysStep;

        /// position of the player
        glm::vec3 playerPosition;
        /// Velocity of the player
        Eigen::Vector3d playerVelocity = Eigen::Vector3d(0,0,0);
        /// Whether the player is falling, e,g. whether below us is a solid block or not
        bool playerFalling = false;
        bool inhibitJankyFix = false;
};
}

#endif
