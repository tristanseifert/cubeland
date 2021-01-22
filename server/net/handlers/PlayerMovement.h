#ifndef NET_HANDLER_PLAYERMOVEMENT_H
#define NET_HANDLER_PLAYERMOVEMENT_H

#include "net/PacketHandler.h"

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>


namespace net::handler {
/**
 * Handles sending chunks as a whole
 */
class PlayerMovement: public PacketHandler {
    public:
        PlayerMovement(ListenerClient *_client) : PacketHandler(_client) {};
        virtual ~PlayerMovement() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        void clientPosChanged(const PacketHeader &, const void *, const size_t);

    private:
        /// max allowable difference between epochs and still consider the value valid
        static constexpr const uint32_t kEpochDiff = 10;
        /// epoch value of the most recently received player position update
        uint32_t lastEpoch = 0;

        /// current player position and view angles
        glm::vec3 position, angles;
        /// whether the position/angles have changed and need to be saved
        bool dirty = false;
};
}


#endif
