#ifndef NET_PACKETHANDLER_H
#define NET_PACKETHANDLER_H

#include <cstddef>

namespace net {
struct PacketHeader;

class ServerConnection;

/**
 * Base class for all objects to handle messages received by server connection workers. An instance
 * is created for each connection.
 */
class PacketHandler {
    public:
        PacketHandler(ServerConnection *_server) : server(_server) {};
        virtual ~PacketHandler() = default;

        virtual bool canHandlePacket(const PacketHeader &header) = 0;
        virtual void handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) = 0;

    protected:
        ServerConnection *server = nullptr;
};
}

#endif
