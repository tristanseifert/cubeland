#ifndef NET_HANDLER_WORLDINFO_H
#define NET_HANDLER_WORLDINFO_H

#include "net/PacketHandler.h"

#include <string>

namespace net::handler {
/**
 * Handles reading world info packets
 */
class WorldInfo: public PacketHandler {
    public:
        WorldInfo(ListenerClient *_client);
        virtual ~WorldInfo() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void authStateChanged() override;

    private:
        void handleGet(const PacketHeader &, const void *, const size_t);
        void sendKey(const std::string &, const uint16_t = 0);
};
}

#endif
