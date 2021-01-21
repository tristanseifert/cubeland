#ifndef NET_HANDLER_PLAYERINFO_H
#define NET_HANDLER_PLAYERINFO_H

#include "net/PacketHandler.h"

#include <string>

namespace net::handler {
/**
 * Handles setting and getting player info in the world.
 */
class PlayerInfo: public PacketHandler {
    public:
        PlayerInfo(ListenerClient *_client);
        virtual ~PlayerInfo() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        void handleGet(const PacketHeader &, const void *, const size_t);
        void handleSet(const PacketHeader &, const void *, const size_t);
};
}

#endif
