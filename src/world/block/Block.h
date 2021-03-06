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
#include <glm/mat4x4.hpp>

namespace particles {
class System;
}

namespace gfx::lights {
class AbstractLight;
}

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

        /// Display name for the block (primarily used in inventory)
        virtual const std::string getDisplayName() const {
            return "(unknown block)";
        }

        /// Controls whether the block is visible in inventory listings
        virtual const bool showsInListing() const {
            return true;
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
        /// Whether the block drops an item
        virtual const bool isCollectable(const glm::ivec3 &pos) const {
            return true;
        }
        /// The ID of the block added to the player's inventory; by default, the block ID
        virtual const uuids::uuid collectableIdFor(const glm::ivec3 &pos) const {
            return this->id;
        }
        /// Number of collectable items dropped.
        virtual const size_t collectableCountFor(const glm::ivec3 &pos) const {
            return 1;
        }

        /**
         * Whether the block is drawn in the alpha blended special pass (where face culling is
         * disabled) or the regular pass with the majority of other blocks.
         *
         * This is best used for mostly transparent blocks like glass where most fragments can be
         * discarded.
         */
        virtual const bool needsAlphaBlending(const glm::ivec3 &pos) const {
            return false;
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

        /**
         * A block of this type is to be rendered at the given world position. This is called for
         * blocks with non-standard models when the chunk they're in is generated for display.
         */
        virtual void blockWillDisplay(const glm::ivec3 &pos) {};

        /**
         * The coordinates used to draw a block's selection can be transformed by a matrix to
         * better match the model. By default, a selection is a 1x1x1 cube, with its origin at the
         * block's origin.
         */
        virtual glm::mat4 getSelectionTransform(const glm::ivec3 &pos) {
            return glm::mat4(1);
        }

    protected:
        void addParticleSystem(std::shared_ptr<particles::System> sys);
        void removeParticleSystem(std::shared_ptr<particles::System> sys);

        void addLight(std::shared_ptr<gfx::lights::AbstractLight> light);
        void removeLight(std::shared_ptr<gfx::lights::AbstractLight> light);

        bool addInventoryItem(const uuids::uuid &id, const size_t count = 1);

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
