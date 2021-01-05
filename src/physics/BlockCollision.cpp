#include "BlockCollision.h"
#include "Engine.h"
#include "Types.h"

#include "render/scene/SceneRenderer.h"
#include "world/chunk/Chunk.h"
#include "world/block/BlockRegistry.h"

#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

// physics library has a few warnings that need to be fixed but we'll just suppress them for now
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#include <reactphysics3d/reactphysics3d.h> 
#pragma GCC diagnostic pop

#include <algorithm>

using namespace physics;
using namespace reactphysics3d;

/**
 * Sets up the block collision handler.
 */
BlockCollision::BlockCollision(Engine *_engine) : engine(_engine) {
    this->blockShape = this->engine->common->createBoxShape(Vector3(.5, .5, .5));
}

/**
 * Clears all of our caches before deallocating. We do not release any of the physics resources
 * we allocated that are still around, since all physics memory is soon to be deallocated
 * anyhow.
 */
BlockCollision::~BlockCollision() {

}



/**
 * Removes any physics bodies corresponding to blocks that are more than a certain distance away
 * from the current player physics body position.
 *
 * Note that this runs only once at the start of a frame; each frame, we may do multiple physics
 * passes. This won't be an issue since the physics engine should automatically ignore these
 * bodies if they're too far away.
 */
void BlockCollision::startFrame() {
    PROFILE_SCOPE(PurgeBlocks);

    // get the position of the body
    const auto &bodyTrans = this->engine->playerBody->getTransform();
    const glm::vec3 bodyPos = vec(bodyTrans.getPosition());

    // discard all bodies that are too far away
    {
        LOCK_GUARD(this->bodiesLock, BodiesLock);
        const auto erased = std::erase_if(this->bodies, [&, bodyPos](const auto &item) {
            const auto& [blockPos, info] = item;
            // if too far away, we need to delete the physics body to ensure it's not dangling around
            if(distance2(bodyPos, glm::vec3(blockPos)) > kBlockMaxDistance) {
                this->removeBlockBody(blockPos, false);
                this->decrementChunkRefCount(blockPos);

                return true;
            }

            return false;
        });

        if(erased) {
            // Logging::trace("Removed {} block bodies due to distance", erased);
        }
    }

    // remove chunk observers that are no longer needed
    const auto erased = std::erase_if(this->chunkObservers, [&](const auto &item) {
        const auto& [chunkPos, token] = item;

        if(!this->activeChunks.contains(chunkPos)) {
            auto chunk = this->engine->scene->getChunk(chunkPos);
            if(chunk) {
                chunk->unregisterChangeCallback(token);
            }

            return true;
        }

        return false;
    });

    if(erased) {
        // Logging::trace("Removed {} chunk observers", erased);
    }
}

/**
 * Removes all blocks.
 */
void BlockCollision::removeAllBlocks() {
    PROFILE_SCOPE(RemoveAllBlocks);
    LOCK_GUARD(this->bodiesLock, BodiesLock);

    // destroy the physics bodies
    for(const auto &[pos, info] : this->bodies) {
        this->removeBlockBody(pos);
        this->decrementChunkRefCount(pos);
    }

    // clear em out
    this->bodies.clear();
}

/**
 * If it exists, removes and dellocates any resources associated with a block's body.
 *
 * @note Assumes the body map lock is already taken.
 */
void BlockCollision::removeBlockBody(const glm::ivec3 &blockPos, const bool remove) {
    if(!this->bodies.contains(blockPos)) return;
    const auto &info = this->bodies[blockPos];

    if(std::holds_alternative<BlockBody>(info)) {
        const auto &body = std::get<BlockBody>(info);
        this->engine->world->destroyRigidBody(body.body);
    }

    if(remove) {
        this->bodies.erase(blockPos);
    }
}

/**
 * Removes the chunk reference count for the chunk containing the given block.
 *
 * This will fail if the given block has a zero ref count.
 */
void BlockCollision::decrementChunkRefCount(const glm::ivec3 &blockPos) {
    glm::ivec2 chunkPos;
    world::Chunk::absoluteToRelative(blockPos, chunkPos);

    auto it = this->activeChunks.find(chunkPos);
    XASSERT(it != this->activeChunks.end(), "Invalid ref count for chunk {} (block {})", chunkPos, blockPos);

    this->activeChunks.erase(it);
}

/**
 * Chunk block change callback
 */
void BlockCollision::chunkBlockDidChange(world::Chunk *chunk, const glm::ivec3 &blockCoord, const world::Chunk::ChangeHints hints) {
    PROFILE_SCOPE(PhysicsChunkChangeCb);

    const auto pos = blockCoord + glm::ivec3(256 * chunk->worldPos.x, 0, 256 * chunk->worldPos.y);

    LOCK_GUARD(this->bodiesLock, BodiesLock);
    this->removeBlockBody(pos);
}

/**
 * Based on the position of the player's physics body, load all blocks in the configured radius
 * around it and creates physics bodies for them.
 */
void BlockCollision::update() {
    PROFILE_SCOPE(BlockCollisionUpdate);

    const auto &scene = this->engine->scene;

    // get the position of the body
    const auto &bodyTrans = this->engine->playerBody->getTransform();
    const glm::ivec3 bodyPos = floor(vec(bodyTrans.getPosition()));

    // check the blocks around us
    LOCK_GUARD(this->bodiesLock, BodiesLock);

    for(int y = -kLoadYRangeN; y <= kLoadYRangeP; y++) {
        for(int z = -kLoadXZRange; z <= kLoadXZRange; z++) {
            for(int x = -kLoadXZRange; x <= kLoadXZRange; x++) {
                // skip if we already have a body generated for this block or it'd be a negative Y
                const auto blockPos = bodyPos + glm::ivec3(x, y, z);
                if(blockPos.y < 0) continue;
                if(this->bodies.contains(blockPos)) continue;

                // get the position of this block, and the chunk that holds it
                glm::ivec2 chunkPos;
                glm::ivec3 blockOff;
                world::Chunk::absoluteToRelative(blockPos, chunkPos, blockOff);

                const auto chunk = scene->getChunk(chunkPos);
                if(!chunk) {
                    continue;
                }

                // set up handler for this block, if required
                if(!this->activeChunks.contains(chunkPos)) {
                    auto tok = chunk->registerChangeCallback(
                            std::bind(&BlockCollision::chunkBlockDidChange, this,
                                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                    this->chunkObservers[chunkPos] = tok;
                }

                this->activeChunks.insert(chunkPos);

                // get the block at this position
                const auto block = chunk->getBlock(blockOff);
                if(!block) {
                    this->bodies[blockPos] = BlockNoCollision();
                    continue;
                }

                // check if it's collidable
                if(world::BlockRegistry::isCollidableBlock(*block, blockPos)) {
                    BlockBody b;

                    Transform transform(vec(blockPos), Quaternion::identity());
                    b.body = this->engine->world->createRigidBody(transform);
                    b.body->setType(BodyType::STATIC);

                    Transform blockTransform(vec(this->blockTranslate), Quaternion::identity());
                    b.collider = b.body->addCollider(this->blockShape, blockTransform);
                    b.collider->setCollisionCategoryBits(Engine::kBlocks);
                    // b.collider->setCollideWithMaskBits(Engine::kBlocks);

                    auto &mat = b.collider->getMaterial();
                    mat.setFrictionCoefficient(kFrictionCoefficient);
                    mat.setBounciness(0);

                    this->bodies[blockPos] = b;
                }
                // it isn't; mark it as such
                else {
                    this->bodies[blockPos] = BlockNoCollision();
                }
            }
        }
    }
}
