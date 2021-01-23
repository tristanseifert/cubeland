#ifndef NET_HANDLER_PLAYERMOVEMENT_H
#define NET_HANDLER_PLAYERMOVEMENT_H

#include "net/PacketHandler.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/vec3.hpp>
#include <cereal/access.hpp>
#include <cpptime.h>

namespace net::handler {
/**
 * Serves as a sort of "bent pipe" for player position updates, so they're propagated to all other
 * players. It also makes sure the player's position is saved/restored correctly.
 */
class PlayerMovement: public PacketHandler {
    public:
        PlayerMovement(ListenerClient *_client);
        virtual ~PlayerMovement();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void authStateChanged() override;

        const bool isDirty() const override {
            return this->dirty;
        }
        void saveData() override {
            this->savePosition();
        }

    private:
        void clientPosChanged(const PacketHeader &, const void *, const size_t);

        void savePosition();
        void broadcastPosition();

    private:
        /// name of the player position saved in the world file
        static const std::string kPositionInfoKey;

        /// world position, saved
        struct SavePos {
            glm::vec3 position, angles;

        private:
            friend class cereal::access;
            template <class Archive> void serialize(Archive &ar) {
                ar(this->position);
                ar(this->angles);
            }
        };

    private:
        /// max allowable difference between epochs and still consider the value valid
        static constexpr const uint32_t kEpochDiff = 10;
        /// epoch value of the most recently received player position update
        uint32_t lastEpoch = 0;

        /// current player position and view angles
        glm::vec3 position, angles;
        /// whether the position/angles have changed and need to be saved
        bool dirty = false;
        /// whether the initial position has been loaded
        bool loadedInitialPos = false;

        /// when set, we need to broadcast data to clientes
        std::atomic_bool needsBroadcast = false;
        /// broadcasting thymer
        CppTime::timer_id broadcastTimerId;
};
}


#endif
