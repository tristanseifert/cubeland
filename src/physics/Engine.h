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

#include <reactphysics3d/mathematics/Transform.h>

struct MetricsGuiMetric;
struct MetricsGuiPlot;

namespace reactphysics3d {
class PhysicsCommon;
class PhysicsWorld;
class RigidBody;
class Collider;
}

namespace render {
class SceneRenderer;
class Camera;
}

namespace physics {
class EngineDebugRenderer;

class Engine {
    public:
        Engine(std::shared_ptr<render::SceneRenderer> &scene, render::Camera *cam);
        ~Engine();

        void startFrame();

        void setPlayerPosition(const glm::vec3 &pos, const glm::vec3 &angles = glm::vec3(0));
        void movePlayer(const glm::vec3 &deltas, const bool jump);

        void setDebugRenderStep(std::shared_ptr<EngineDebugRenderer> &dbg);

    private:
        void updateDebugFlags();
        void drawDebugUi();

    private:
        /// physics engine time step, in seconds
        constexpr static const float kTimeStep = 1./60.;

        /// Force, in Newtons, to apply as movement.
        constexpr static const float kMovementForce = 25.;

        /// Height of the player, in meters
        constexpr static const double kPlayerHeight = 1.92;
        /// Mass of the player (in kg)
        constexpr static const double kPlayerMass = 87.5;
        /// Velocity to apply when jumping
        constexpr static const double kJumpVelocity = 2.;

    private:
        using Event = std::variant<std::monostate>;

    private:
        /// this is the source of all data for the physics engine
        reactphysics3d::PhysicsCommon *common = nullptr;
        /// actual physics world that all the things take place in
        reactphysics3d::PhysicsWorld *world = nullptr;

        /// rigid body representing the player
        reactphysics3d::RigidBody *playerBody = nullptr;
        /// collider for the shape representing the player collision area
        reactphysics3d::Collider *playerCollider = nullptr;
        /// last transform of the player, used for interpolation
        reactphysics3d::Transform lastPlayerTransform;

        /// get chunk data from this scene
        std::shared_ptr<render::SceneRenderer> scene = nullptr;
        /// camera for viewing
        render::Camera *camera = nullptr;

        /// number of physics steps executed
        size_t numSteps = 0;
        /// time of the last physics engine step
        std::chrono::high_resolution_clock::time_point lastFrameTime;
        /// frame difference accumulator, in seconds
        float stepAccumulator = 0.;

    private:
        /// whether the debug UI is shown
        bool showDebugWindow = true;

        MetricsGuiPlot *mPlot;
        MetricsGuiMetric *mAccumulator, *mStepTime;

        std::shared_ptr<EngineDebugRenderer> dbgStep = nullptr;
        bool dbgUpdateNeeded = true;
        bool dbgDrawInfo = true;
        bool dbgDrawColliderAabb = false;
        bool dbgDrawColliderBroadphase = true;
        bool dbgDrawCollisionShape = true;
        bool dbgDrawContactPoints = true;
        bool dbgDrawContactNormals = false;
};
}

#endif
