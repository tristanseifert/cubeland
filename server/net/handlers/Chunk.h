#ifndef NET_HANDLER_CHUNK_H
#define NET_HANDLER_CHUNK_H

#include "net/PacketHandler.h"

#include <uuid.h>
#include <glm/vec2.hpp>
#include <glm/gtx/hash.hpp>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace util {
class LZ4;
}

namespace world {
struct Chunk;
struct ChunkSlice;
}

namespace net::handler {
/**
 * Handles sending chunks as a whole
 */
class ChunkLoader: public PacketHandler {
    public:
        ChunkLoader(ListenerClient *_client) : PacketHandler(_client) {};
        virtual ~ChunkLoader();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        struct Maps {
            /**
             * Mapping of grid 16-bit values to block UUIDs
             */
            std::unordered_map<uuids::uuid, uint16_t> gridUuidMap;

            /**
             * For each of the chunk's row block type maps (which map the 8-bit row values to the
             * block UUID) we create a version that maps to a 16-bit value that's stored in a
             * slice's wire representation.
             */
            std::vector<std::unordered_map<uint8_t, uint16_t>> rowToGrid;
        };

    private:
        void handleGet(const PacketHeader &, const void *, const size_t);
        void sendSlices(const std::shared_ptr<world::Chunk> &);
        void sendSlice(const std::shared_ptr<world::Chunk> &, const Maps &, const world::ChunkSlice *, const size_t);
        void sendCompletion(const std::shared_ptr<world::Chunk> &, const size_t);

    private:
        /// lock protecting chunk cache
        static std::mutex cacheLock;
        /// cache map
        static std::unordered_map<glm::ivec2, std::weak_ptr<world::Chunk>> cache;

        /// lock protecting duplicates set
        std::mutex dupesLock;
        /// set containing chunk positions we're currently working on
        std::unordered_set<glm::ivec2> dupes;

        std::mutex completionsLock;
        /// mapping of chunk pos -> completion future
        std::unordered_map<glm::ivec2, std::future<void>> completions;
};
}

#endif
