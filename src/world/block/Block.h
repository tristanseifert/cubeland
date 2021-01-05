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

#include "BlockRegistry.h"

#include <cstdint>
#include <uuid.h>

#include <glm/vec3.hpp>

namespace world {
struct Chunk;

class Block {
    public:
        enum BlockFlags: uint32_t {
            kFlagsNone = 0,

            /// exposed edges
            kExposureMask       = 0x3F,

            kExposedYPlus       = (1 << 0),
            kExposedYMinus      = (1 << 1),
            kExposedXPlus       = (1 << 2),
            kExposedXMinus      = (1 << 3),
            kExposedZPlus       = (1 << 4),
            kExposedZMinus      = (1 << 5),
        };

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

        /// Returns the texture ID used in the inventory UI
        virtual const BlockRegistry::TextureId getInventoryIcon() const {
            return this->inventoryIcon;
        }

        /// Number of ticks required to destroy the block (0 = instant)
        virtual const size_t destroyTicks(const glm::ivec3 &pos) const {
            return 0;
        }

        /// Whether the block is fully opaque
        virtual const bool isOpaque() const {
            return true;
        }
        /// Whether the block can be collided with
        virtual const bool isCollidable(const glm::ivec3 &pos) const {
            return true;
        }

        /// Whether the block may be selected
        virtual const bool isSelectable(const glm::ivec3 &pos) const {
            return true;
        }

        /**
         * Called for every game tick. Any block specific things (such as aging blocks) should be
         * performed in response.
         */
        virtual void tickHandler() {}

        /**
         * Returns the 16-bit block appearance to use for drawing the block at the given world
         * position. When invoked, the drawing code has already evaluated the UUID and determined
         * this instance should draw it, so you really only need to decide if the block has a
         * special appearance.
         *
         * For convenience, a set of flags is included indicating what edges the block is exposed
         * on.
         */
        virtual uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) = 0;

        /**
         * Returns the 16-bit model ID to use for drawing this block. This is of interest mostly to
         * non-solid blocks.
         *
         * A value of 0 uses the standard block/cube model.
         */
        virtual uint16_t getModelId(const glm::ivec3 &pos, const BlockFlags flags) {
            return 0;
        }

        /// Whether this block type is interested in chunk load/unload notifications
        virtual const bool wantsChunkLoadNotifications() const { return false; }
        /// A chunk has started to be loaded
        virtual void chunkWasLoaded(std::shared_ptr<Chunk> chunk) {};
        /// A chunk is about to be unloaded
        virtual void chunkWillUnload(std::shared_ptr<Chunk> chunk) {};

    protected:
        /// Unique identifier for the block
        uuids::uuid id;

        /// Internal rdns-style name
        std::string internalName;

        /// inventory icon texture id
        BlockRegistry::TextureId inventoryIcon;
};

// proper bitset OR for block flags. (XXX: extend if we ever use more than 32 bits)
inline Block::BlockFlags operator|(Block::BlockFlags a, Block::BlockFlags b) {
    return static_cast<Block::BlockFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline Block::BlockFlags operator|=(Block::BlockFlags &a, Block::BlockFlags b) {
    return (Block::BlockFlags &) ((uint32_t &) a |= (uint32_t) b);
}

}

#endif
