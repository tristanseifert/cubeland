#ifndef NET_HANDLER_CHUNK_H
#define NET_HANDLER_CHUNK_H

#include "net/PacketHandler.h"

#include <cstddef>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/gtx/hash.hpp>

namespace world {
struct Chunk;
}

namespace net::handler {

class ChunkLoader: public PacketHandler {
    public:
        ChunkLoader(ServerConnection *_server);
        virtual ~ChunkLoader();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        std::future<std::shared_ptr<world::Chunk>> get(const glm::ivec2 &pos);

    private:
        void handleCompletion(const PacketHeader &, const void *, const size_t);

    private:
        /// lock over the promises list
        std::mutex requestsLock;
        /// outstanding requests
        std::unordered_map<glm::ivec2, std::promise<std::shared_ptr<world::Chunk>>> requests;

        /// lock to protect in progress chunks
        std::mutex inProgressLock;
        /// in progress chunks
        std::unordered_map<glm::ivec2, std::shared_ptr<world::Chunk>> inProgress;
};
}

#endif
