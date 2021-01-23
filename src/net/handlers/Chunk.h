#ifndef NET_HANDLER_CHUNK_H
#define NET_HANDLER_CHUNK_H

#include "net/PacketHandler.h"

#include <atomic>
#include <condition_variable>
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

namespace net::message {
struct ChunkSliceData;
struct ChunkCompletion;
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

        void abortAll();

    private:
        void handleSlice(const PacketHeader &, const void *, const size_t);
        void process(const message::ChunkSliceData &);

        void handleCompletion(const PacketHeader &, const void *, const size_t);
        void process(const message::ChunkCompletion &);

    private:
        /// lock over the promises list
        std::mutex requestsLock;
        /// outstanding requests
        std::unordered_map<glm::ivec2, std::promise<std::shared_ptr<world::Chunk>>> requests;

        /// lock to protect in progress chunks
        std::mutex inProgressLock;
        /// in progress chunks
        std::unordered_map<glm::ivec2, std::shared_ptr<world::Chunk>> inProgress;

        /**
         * Count of slices processed, by chunk position. We've got a lock and condition variable
         * that's signalled any time this changes. The completion callbacks will block on this to
         * ensure all slices in the chunk have finished processing.
         */
        std::unordered_map<glm::ivec2, size_t> counts;
        std::condition_variable countsCond;
        std::mutex countsLock;

        std::atomic_bool acceptGets = true;
};
}

#endif
