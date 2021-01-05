/**
 * Builds the physics environment around the player from chunk data that's been loaded. This will
 * create a "cube" of blocks around the player, which saves lots of simulation time since we
 * don't really care about collisions with far distant blocks.
 */
#ifndef PHYSICS_BLOCKCOLLISION_H
#define PHYSICS_BLOCKCOLLISION_H

#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

#include "world/chunk/Chunk.h"

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
        /// - y range of blocks to create for collision
        constexpr static const int kLoadYRangeN = 2;
        /// + y range of blocks to create for collision
        constexpr static const int kLoadYRangeP = 3;
        /// +/- x/z range of blocks to create for collision
        constexpr static const int kLoadXZRange = 4;

        /// Block bodies further than this distance are discarded (squared)
        constexpr static const float kBlockMaxDistance = 8.*8.;

        /// Default friction coefficient for blocks
        constexpr static const float kFrictionCoefficient = 0.25;

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
        void removeBlockBody(const glm::ivec3 &blockPos, const bool remove = true);
        void decrementChunkRefCount(const glm::ivec3 &blockPos);
        void chunkBlockDidChange(world::Chunk *chunk, const glm::ivec3 &blockCoord, const world::Chunk::ChangeHints hints);

    private:
        Engine *engine = nullptr;

        /// collision shape for a 1x1x1 cube
        reactphysics3d::CollisionShape *blockShape = nullptr;
        /// translation for the block shape
        glm::vec3 blockTranslate = glm::vec3(.5);

        /// lock protecting the bodies map
        std::mutex bodiesLock;
        /// physics bodies generated for blocks. key is block coordinate
        std::unordered_map<glm::ivec3, BodyInfo> bodies;

        /// pseudo reference count of blocks inside a chunk. used to remove unneeded observers
        std::unordered_multiset<glm::ivec2> activeChunks;
        /// IDs of chunk observers
        std::unordered_map<glm::ivec2, uint32_t> chunkObservers;
};
}

#endif
