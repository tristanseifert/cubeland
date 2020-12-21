/**
 * All blocks registered into the game are represented by one of these objects.
 *
 * They define its behaviors (interactability, physics, etc.) as well as its appearance (by means
 * of textures to apply).
 */
#ifndef WORLD_BLOCK_BLOCK_H
#define WORLD_BLOCK_BLOCK_H

#include <uuid.h>

namespace world {
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

    protected:
        /// Unique identifier for the block
        uuids::uuid id;

        /// Internal rdns-style name
        std::string internalName;
};
}

#endif
