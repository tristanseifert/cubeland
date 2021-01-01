/**
 * Builds the physics environment around the player from chunk data that's been loaded. This will
 * create a "cube" of blocks around the player, which saves lots of simulation time since we
 * don't really care about collisions with far distant blocks.
 *
 * XXX: Currently, the physics blocks aren't updated as the chunks are modified.
 */
#ifndef PHYSICS_BLOCKCOLLISION_H
#define PHYSICS_BLOCKCOLLISION_H

#include <variant>
#include <unordered_map>

#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

namespace reactphysics3d {
class CollisionShape;
class RigidBody;
class Collider;
}

namespace physics {
class Engine;

class BlockCollision {
    friend class Engine;

    public:
        BlockCollision(Engine *physics);
        ~BlockCollision();

        void startFrame();
        void update();

        void removeAllBlocks();

    private:
        /// +/- y range of blocks to create for collision
        constexpr static const int kLoadYRange = 4;
        /// +/- x/z range of blocks to create for collision
        constexpr static const int kLoadXZRange = 5;

        /// Block bodies further than this distance are discarded (squared)
        constexpr static const float kBlockMaxDistance = 10.*10.;

    private:
        /// Holds block body information (for blocks that want collision)
        struct BlockBody {
            /// rigid body that can be collided with
            reactphysics3d::RigidBody *body = nullptr;
            /// collider handle for the block
            reactphysics3d::Collider *collider = nullptr;
        };

        /// Holds info for blocks that exist, but do not want collision
        struct BlockNoCollision {
            uint8_t dummy;
        };

        using BodyInfo = std::variant<BlockNoCollision, BlockBody>;

    private:
        Engine *engine = nullptr;

        /// collision shape for a 1x1x1 cube
        reactphysics3d::CollisionShape *blockShape = nullptr;
        /// translation for the block shape
        glm::vec3 blockTranslate = glm::vec3(.5);

        /// physics bodies generated for blocks. key is block coordinate
        std::unordered_map<glm::ivec3, BodyInfo> bodies;
};
}

#endif
