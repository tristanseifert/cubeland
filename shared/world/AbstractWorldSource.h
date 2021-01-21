#ifndef SHARED_WORLD_ABSTRACTWORLDSOURCE_H
#define SHARED_WORLD_ABSTRACTWORLDSOURCE_H

#include <cstddef>
#include <future>
#include <memory>
#include <string>

#include <uuid.h>
#include <glm/vec2.hpp>

namespace world {
struct Chunk;

/**
 * This is the interface all world sources implement; the world source is the origin for all chunk
 * data, as well as metadata about the player or world itself. This data may come from a file on
 * disk, be generated on demand, or even over the network based on the underlying implementation.
 */
class AbstractWorldSource {
    public:
        virtual ~AbstractWorldSource() = default;

        /// Returns a chunk at the given world chunk position
        virtual std::future<std::shared_ptr<Chunk>> getChunk(int x, int z) = 0;

        /// Set the value of a player info key.
        virtual std::future<void> setPlayerInfo(const uuids::uuid &id, const std::string &key, const std::vector<char> &value) = 0;
        /// Reads the value of a player info key.
        virtual std::promise<std::vector<char>> getPlayerInfo(const uuids::uuid &id, const std::string &key) = 0;

        /// Reads the value of a world info key.
        virtual std::promise<std::vector<char>> getWorldInfo(const std::string &key) = 0;

        /**
         * Request that all chunks are saved; this may mean queued to write to disk or sent over
         * the network, but the actual transfer may not have finished.
         */
        virtual void flushDirtyChunksSync() = 0;
};
}

#endif
