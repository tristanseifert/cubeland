#ifndef NET_HANDLER_BLOCKCHANGE_H
#define NET_HANDLER_BLOCKCHANGE_H

#include "net/PacketHandler.h"

#include <world/chunk/Chunk.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

namespace world {
class RemoteSource;
}

namespace net::handler {
class BlockChange: public PacketHandler {
    public:
        BlockChange(ServerConnection *_server) : PacketHandler(_server) {};
        virtual ~BlockChange() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void startChunkNotifications(const std::shared_ptr<world::Chunk> &);
        void stopChunkNotifications(const std::shared_ptr<world::Chunk> &);

    private:
        void updateChunks(const PacketHeader &, const void *, const size_t);

        void chunkChanged(world::Chunk *, const glm::ivec3 &, const world::Chunk::ChangeHints, const uuids::uuid &);

    private:
        // observers attached to chunks
        std::mutex observersLock;
        std::unordered_map<glm::ivec2, uint32_t> observers;

        // actual chunks being shown
        std::mutex chunksLock;
        std::unordered_map<glm::ivec2, std::shared_ptr<world::Chunk>> chunks;

        /// when set, we don't generate change reports
        std::atomic_bool inhibitChangeReports = false;
};
}

#endif
