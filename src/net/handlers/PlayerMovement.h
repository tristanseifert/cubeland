#ifndef NET_HANDLER_PLAYERMOVEMENT_H
#define NET_HANDLER_PLAYERMOVEMENT_H

#include "net/PacketHandler.h"

#include <cstdint>

#include <glm/vec3.hpp>

namespace world {
class RemoteSource;
}

namespace net::handler {

class PlayerMovement: public PacketHandler {
    friend class world::RemoteSource;

    public:
        PlayerMovement(ServerConnection *_server) : PacketHandler(_server) {};
        virtual ~PlayerMovement() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void positionChanged(const glm::vec3 &pos, const glm::vec3 &angles);

    private:
        void otherPlayerMoved(const PacketHeader &, const void *, const size_t);
        void handleInitialPos(const PacketHeader &, const void *, const size_t);

    private:
        /// we've received the initial position message
        bool hasInitialPos = false;
        /// most recent position and angles (only set by initial message frame atm)
        glm::vec3 position, angles;

        /// epoch value to insert into outgoing position update packets; increments by one
        uint32_t epoch = 1;
};
}

#endif
