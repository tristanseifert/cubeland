#ifndef NET_PACKETHANDLER_H
#define NET_PACKETHANDLER_H

#include <cstddef>

namespace net {
struct PacketHeader;

class ListenerClient;

/**
 * Base class for all objects to handle messages received by server client workers. An instance is
 * created for each client.
 */
class PacketHandler {
    public:
        PacketHandler(ListenerClient *_client) : client(_client) {};
        virtual ~PacketHandler() = default;

        virtual bool canHandlePacket(const PacketHeader &header) = 0;
        virtual void handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) = 0;

        /// authentication state of the connection changed
        virtual void authStateChanged() {};

        /// whether we need data to be saved
        virtual const bool isDirty() const {
            return false;
        }
        /// periodic tick to save data
        virtual void saveData() {};

    protected:
        ListenerClient *client = nullptr;
};
}

#endif
