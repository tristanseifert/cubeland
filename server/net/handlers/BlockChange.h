#ifndef NET_HANDLER_BLOCKCHANGE_H
#define NET_HANDLER_BLOCKCHANGE_H

#include "net/PacketHandler.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <net/EPBlockChange.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>
#include <cereal/access.hpp>
#include <cpptime.h>
#include <blockingconcurrentqueue.h>

namespace world {
struct Chunk;
}

namespace net {
class Listener;
}

namespace net::handler {
class Chunk;

/**
 * Receives block change notifications from the client, then applies them to the chunk and re-
 * broadcasts the change to all other clients.
 */
class BlockChange: public PacketHandler {
    friend class net::Listener;

    public:
        BlockChange(ListenerClient *_client) : PacketHandler(_client) {}
        virtual ~BlockChange();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        /// observes the given chunk for changes
        void addObserver(const std::shared_ptr<world::Chunk> &);

    private:
        void handleChange(const PacketHeader &, const void *, const size_t);
        void removeObserver(const PacketHeader &, const void *, const size_t);

    private:
        /// types of operations performed by broadcast thread
        enum class WorkerOp: uint8_t {
            NoOp,
            BlockChange,
        };

        struct BroadcastItem {
            WorkerOp op = WorkerOp::NoOp;

            // block changes
            std::vector<message::BlockChangeInfo> changes;
        };

    private:
        static void startBroadcaster(net::Listener *);
        static void stopBroadcaster();

        static void broadcasterMain(net::Listener *);
        static void broadcasterHandleChanges(net::Listener *, const BroadcastItem &);

    private:
        static std::unique_ptr<std::thread> broadcastThread;
        static std::atomic_bool broadcastRun;
        static moodycamel::BlockingConcurrentQueue<BroadcastItem> broadcastQueue;

        std::mutex chunksLock;
        std::unordered_map<glm::ivec2, std::shared_ptr<world::Chunk>> chunks;
};
}

#endif
