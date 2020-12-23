/**
 * All blocks registered into the game are represented by one of these objects.
 *
 * They define its behaviors (interactability, physics, etc.) as well as its appearance (by means
 * of textures to apply).
 *
 * You will usually register a single instance of this type to handle ALL occurrences of this block
 * in the game. If blocks are more complex than static display, you can internally keep track of
 * per block data by hooking the notifications for chunks being loaded and unloaded.
 *
 * Note that there is no guarantee as to what threads any of these methods are run on. In addition,
 * the engine may call methods in the same implementation from several threads simultaneously, so
 * shared data should be adequately protected.
 *
 * Additionally, there is no guarantee that the block handler is invoked for ALL blocks in a chunk;
 * it is extremely likely that the engine will cull most blocks away and invoke the handler only
 * for those blocks that are visible.
 */
#ifndef WORLD_BLOCK_BLOCK_H
#define WORLD_BLOCK_BLOCK_H

#include <uuid.h>

#include <glm/vec3.hpp>

namespace world {
struct Chunk;

class Block {
    public:
        virtual ~Block() = default;

        /// Gets the internal name (reverse-DNS style) of the block
        virtual const std::string getInternalName() const {
            return this->internalName;
        }
        /// Gets the block's UUID.
        virtual const uuids::uuid getId() const {
            return this->id;
        }

        /// Returns the 16-bit block type id to use for drawing the block at the given world pos
        virtual uint16_t getBlockId(const glm::ivec3 &pos) = 0;

        /// Whether this block type is interested in chunk load/unload notifications
        virtual const bool wantsChunkLoadNotifications() const { return false; }
        /// A chunk has started to be loaded
        virtual void chunkWasLoaded(std::shared_ptr<Chunk> &chunk) {};
        /// A chunk is about to be unloaded
        virtual void chunkWillUnload(std::shared_ptr<Chunk> &chunk) {};

    protected:
        /// Unique identifier for the block
        uuids::uuid id;

        /// Internal rdns-style name
        std::string internalName;
};
}

#endif
