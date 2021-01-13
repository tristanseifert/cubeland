#ifndef WORLD_WORLDREADER_H
#define WORLD_WORLDREADER_H

#include <future>
#include <memory>
#include <vector>
#include <string>

#include <uuid.h>
#include <glm/vec4.hpp>

namespace world {
struct Chunk;

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

        /**
         * Loads data for the given chunk.
         */
        virtual std::promise<std::shared_ptr<Chunk>> getChunk(int x, int z) = 0;

        /**
         * Writes the given chunk to the world file. Existing chunks will be updated; if there is
         * not a chunk at this chunk's location, a new one is allocated.
         *
         * @return A promise that indicates whether the chunk was written or not.
         */
        virtual std::promise<bool> putChunk(std::shared_ptr<Chunk> chunk) = 0;

        /**
         * Reads a player info key for the given player and key name pair.
         */
        virtual std::promise<std::vector<char>> getPlayerInfo(const uuids::uuid &player, const std::string &key) = 0;

        /**
         * Sets a given player's player info key value.
         */
        virtual std::promise<void> setPlayerInfo(const uuids::uuid &player, const std::string &key, const std::vector<char> &data) = 0;
        /**
         * Reads a particular world info key.
         */
        virtual std::promise<std::vector<char>> getWorldInfo(const std::string &key) = 0;

        /**
         * Sets a given world info key.
         */
        virtual std::promise<void> setWorldInfo(const std::string &key, const std::vector<char> &data) = 0;
        /**
         * Sets a string world info key.
         */
        virtual std::promise<void> setWorldInfo(const std::string &key, const std::string &data) {
            std::vector<char> bytes(data.begin(), data.end());
            return this->setWorldInfo(key, bytes);
        }
};
}

#endif
