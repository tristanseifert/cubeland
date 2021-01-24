#ifndef NET_HANDLER_CHAT_H
#define NET_HANDLER_CHAT_H

#include "net/PacketHandler.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace net::handler {
class Chat: public PacketHandler {
    public:
        Chat(ServerConnection *_server) : PacketHandler(_server) {};
        virtual ~Chat() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        void message(const PacketHeader &, const void *, const size_t);

        void playerJoined(const PacketHeader &, const void *, const size_t);
        void playerLeft(const PacketHeader &, const void *, const size_t);
};
}

#endif
