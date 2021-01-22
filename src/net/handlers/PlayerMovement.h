#ifndef NET_HANDLER_PLAYERMOVEMENT_H
#define NET_HANDLER_PLAYERMOVEMENT_H

#include "net/PacketHandler.h"

#include <cstdint>

#include <glm/vec3.hpp>

namespace net::handler {

class PlayerMovement: public PacketHandler {
    public:
        PlayerMovement(ServerConnection *_server) : PacketHandler(_server) {};
        virtual ~PlayerMovement() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void positionChanged(const glm::vec3 &pos, const glm::vec3 &angles);

    private:
        void otherPlayerMoved(const PacketHeader &, const void *, const size_t);

    private:
        /// epoch value to insert into outgoing position update packets; increments by one
        uint32_t epoch = 1;
};
}

#endif
