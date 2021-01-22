#include "PlayerMovement.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPPlayerMovement.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Handle player movement packets.
 */
bool PlayerMovement::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointPlayerMovement) return false;
    // it musn't be more than the max value
    if(header.type >= kPlayerPositionTypeMax) return false;

    return true;
}

/**
 * Handles a received packet. This should only really be the client -> server player movement
 * packets.
 */
void PlayerMovement::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        case kPlayerPositionChanged:
            this->clientPosChanged(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid chunk packet type: ${:02x}", header.type));
    }
}



/**
 * Handles a packet indicating that the connected client's position changed.
 */
void PlayerMovement::clientPosChanged(const PacketHeader &, const void *payload,
        const size_t payloadLen) {
    // deserialize the message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    PlayerPositionChanged request;
    iArc(request);

    // discard if this packet is too old
    if(request.epoch < this->lastEpoch && (this->lastEpoch - request.epoch) < kEpochDiff) {
        Logging::debug("Discarding player position update from {} (epoch ${:x}, last ${:x})",
                this->client->getClientId(), request.epoch, this->lastEpoch);
        return;
    }

    this->lastEpoch = request.epoch;

    // store position
    this->position = request.position;
    this->angles = request.angles;
    this->dirty = true;

    Logging::trace("New position for {}: {} {}", this->client->getClientId(), request.position, request.angles);

    // TODO: broadcast to other clients
}
