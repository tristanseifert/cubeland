#ifndef NET_HANDLER_CHUNK_H
#define NET_HANDLER_CHUNK_H

#include "net/PacketHandler.h"

#include <glm/vec2.hpp>
#include <glm/gtx/hash.hpp>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
        ChunkLoader(ListenerClient *_client);
        virtual ~ChunkLoader() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        void handleGet(const PacketHeader &, const void *, const size_t);
        void sendSlices(const std::shared_ptr<world::Chunk> &);
        void sendSlice(const std::shared_ptr<world::Chunk> &, const world::ChunkSlice *);
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
};
}

#endif
