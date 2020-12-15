#ifndef WORLD_WORLDREADER_H
#define WORLD_WORLDREADER_H

#include <future>

#include <glm/vec4.hpp>

namespace world {
/**
 * Interface exported by all world reading implementations.
 *
 * This allows the rest of the game logic to easily operate with worlds read from file, over the
 * network, or other places.
 */
class WorldReader {
    public:
        virtual ~WorldReader() = default;

    public:
        /**
         * Determines whether we have a chunk for the given X/Z coordinate.
         *
         * @note Coordinates are chunk relative; e.g. they increment by 1, not 256.
         */
        virtual std::promise<bool> chunkExists(int x, int z) = 0;

        /**
         * Returns the extents of the world.
         *
         * In other words, this returns the minimum and maximum of X/Z coordinates used by chunks;
         * from this, you can establish the maximum bounds of the world. Not all chunks inside this
         * rectangular region might be populated, however.
         */
        virtual std::promise<glm::vec4> getWorldExtents() = 0;
};
}

#endif
