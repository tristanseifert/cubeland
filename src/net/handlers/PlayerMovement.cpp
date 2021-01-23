#include "PlayerMovement.h"
#include "net/ServerConnection.h"

#include <net/PacketTypes.h>
#include <net/EPPlayerMovement.h>

#include <Logging.h>
#include <io/Format.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Checks if we can handle the given packet.
 */
bool PlayerMovement::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointPlayerMovement) return false;
    // it musn't be more than the max value
    if(header.type >= kPlayerPositionTypeMax) return false;

    return true;
}


/**
 * Handles world info packets.
 */
void PlayerMovement::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    PROFILE_SCOPE(PlayerMovement);

    switch(header.type) {
        case kPlayerPositionBroadcast:
            this->otherPlayerMoved(header, payload, payloadLen);
            break;

        case kPlayerPositionInitial:
            this->handleInitialPos(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid player movement packet type: ${:02x}", header.type));
    }
}

/**
 * Another player on the server has moved; update internal state.
 */
void PlayerMovement::otherPlayerMoved(const PacketHeader &, const void *payload,
        const size_t payloadLen) {
    // try to deserialize the message

    // deal with it

    // idk fam
}



/**
 * Transmits a position change packet.
 */
void PlayerMovement::positionChanged(const glm::vec3 &pos, const glm::vec3 &angles) {
    // build packet
    PlayerPositionChanged delta;
    delta.epoch = this->epoch++;
    delta.position = pos;
    delta.angles = angles;

    // send it
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(delta);

    // send it
    this->server->writePacket(kEndpointPlayerMovement, kPlayerPositionChanged, oStream.str());
}

/**
 * Handles a received initial position packet. We'll simply store the position and angles for
 * later.
 */
void PlayerMovement::handleInitialPos(const PacketHeader &, const void *payload, const size_t payloadLen) {
    // deserialize message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    PlayerPositionInitial initial;
    iArc(initial);

    // lastly, set the state so the world source can pick up this position
    this->position = initial.position;
    this->angles = initial.angles;

    this->hasInitialPos = true;
}
