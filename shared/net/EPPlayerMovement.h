#ifndef SHARED_NET_EPPLAYERMOVEMENT_H
#define SHARED_NET_EPPLAYERMOVEMENT_H

#include <cstddef>

#include <glm/vec2.hpp>
#include <uuid.h>
#include <io/Serialization.h>

#include <cereal/access.hpp>
#include <cereal/types/string.hpp>

namespace net::message {
/**
 * Message types for the player movement endpoint
 */
enum PlayerMovementMsgType: uint8_t {
    /// client -> server; the local player moved
    kPlayerPositionChanged              = 0x01,
    /// server -> client; player position broacast
    kPlayerPositionBroadcast            = 0x02,
    /// server -> client; unsolicited initial position message
    kPlayerPositionInitial              = 0x03,

    kPlayerPositionTypeMax,
};


/**
 * Client to server message indicating that our player has moved.
 */
struct PlayerPositionChanged {
    /// timestamp/identifier; used to reject out-of-order/older updates
    uint32_t epoch;

    /// origin of the player bounding volume
    glm::vec3 position;
    /// camera angles currently used
    glm::vec3 angles;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->epoch);
            ar(this->position);
            ar(this->angles);
        }
};


/**
 * Initial position message; this is sent unsolicited after successful authentication.
 */
struct PlayerPositionInitial {
    /// origin of the player bounding volume
    glm::vec3 position;
    /// camera angles currently used
    glm::vec3 angles;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->position);
            ar(this->angles);
        }
};

}

#endif
