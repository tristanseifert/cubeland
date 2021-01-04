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
#include <mutex>

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

namespace chunk {
class Globule;
}
}

namespace physics {
class EngineDebugRenderer;
class BlockCollision;

class Engine {
    friend class BlockCollision;

    public:
        Engine(std::shared_ptr<render::SceneRenderer> &scene, render::Camera *cam);
        ~Engine();

        void startFrame();

        void setPlayerPosition(const glm::vec3 &pos, const glm::vec3 &angles = glm::vec3(0));
        void movePlayer(const glm::vec3 &deltas, const bool jump);

        void setDebugRenderStep(std::shared_ptr<EngineDebugRenderer> &dbg);

        /// Returns a reference to the physics common object
        reactphysics3d::PhysicsCommon *getCommon() {
            return this->common;
        }
        /// Returns a reference to the physics world object
        reactphysics3d::PhysicsWorld *getWorld() {
            return this->world;
        }

    public:
        /// Defines the meaning of collision bitmasks
        enum CollisionMask: uint32_t {
            /// Characters
            kCharacters                 = 0x0000000F,
            /// Player character
            kPlayerCharacter            = (1 << 0),

            /// Environmental objects mask
            kEnvironment                = 0x000000F0,
            /// Chunk (e.g. blocks)
            kBlocks                     = (1 << 4),

            /// Particle system objects
            kParticles                  = (1 << 8),
        };

    private:
        void singleStep();

        void updateDebugFlags();
        void drawDebugUi();

    private:
        /// gravity vector for the world (in m/s)
        constexpr static const glm::vec3 kWorldGravity = glm::vec3(0, -9.81, 0);

        /// physics engine time step, in seconds
        constexpr static const float kTimeStep = 1./60.;

        /// Force, in Newtons, to apply as movement.
        constexpr static const float kMovementForce = 574.;
        /// Force to apply when jumping in the +Y direction (in N)
        constexpr static const float kJumpForce = 27740.;

        /// Height of the player, in meters
        constexpr static const float kPlayerHeight = 1.92;
        /// Mass of the player (in kg)
        constexpr static const float kPlayerMass = 87.5;
        /// Linear damping factor for player movement
        constexpr static const float kPlayerLinearDamping = 0.25;

        /// Bounciness of the player
        constexpr static const float kPlayerBounciness = 0.174;
        /// Friction coefficient for player
        constexpr static const float kPlayerFriction = 0.274;


    private:
        using Event = std::variant<std::monostate>;

    private:
        /// lock used for accessing the physics engine
        std::mutex engineLock;
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

        /// handler for block collisions
        BlockCollision *blockCol = nullptr;

        /// number of physics steps executed
        size_t numSteps = 0;
        /// time of the last physics engine step
        std::chrono::high_resolution_clock::time_point lastFrameTime;
        /// frame difference accumulator, in seconds
        float stepAccumulator = 0.;

        /// jump inhibit flag
        bool jump = false;

    private:
        /// whether the debug UI is shown
        bool showDebugWindow = false;
        uint32_t menuItem = 0;

        MetricsGuiPlot *mPlot;
        MetricsGuiMetric *mAccumulator, *mStepTime;

        std::shared_ptr<EngineDebugRenderer> dbgStep = nullptr;
        bool dbgUpdateNeeded = false;
        bool dbgDrawInfo = false;
        bool dbgDrawColliderAabb = false;
        bool dbgDrawColliderBroadphase = false;
        bool dbgDrawCollisionShape = true;
        bool dbgDrawContactPoints = true;
        bool dbgDrawContactNormals = false;
};
}

#endif
