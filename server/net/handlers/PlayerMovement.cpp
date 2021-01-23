#include "PlayerMovement.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPPlayerMovement.h>
#include <io/ConfigManager.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

const std::string PlayerMovement::kPositionInfoKey = "server.player.position";

/**
 * Register the broadcast timer.
 */
PlayerMovement::PlayerMovement(ListenerClient *_client) : PacketHandler(_client) {
    const auto updateFreq = io::ConfigManager::getUnsigned("proto.positionBroadcastInterval", 74);
    const auto interval = std::chrono::milliseconds(updateFreq);

    this->broadcastTimerId = this->client->getListener()->addRepeatingTimer(interval, [&]() {
        this->broadcastPosition();
    });
}


/**
 * Removes the registered broadcast timer.
 */
PlayerMovement::~PlayerMovement() {
    this->client->getListener()->removeTimer(this->broadcastTimerId);
}

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

    // set broadcast flag
    this->needsBroadcast = true;
}



/**
 * Serializes the current position and angle to the world file.
 */
void PlayerMovement::savePosition() {
    auto world = this->client->getWorld();
    auto id = *this->client->getClientId();

    // build the info struct
    SavePos p;
    p.position = this->position;
    p.angles = this->angles;

    // serialize and write out
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(p);

    const auto &str = oStream.str();
    std::vector<char> bytes(str.begin(), str.end());

    auto fut = world->setPlayerInfo(id, kPositionInfoKey, bytes);
    fut.get();

    // clear dirty flag
    this->dirty = false;
}

/**
 * Auth state callback; we'll load the position from the world file at this time.
 */
void PlayerMovement::authStateChanged() {
    if(this->loadedInitialPos) return;
    if(!this->client->getClientId()) return;

    auto id = *this->client->getClientId();

    // try to load the meeper
    auto world = this->client->getWorld();
    auto prom = world->getPlayerInfo(id, kPositionInfoKey);
    auto value = prom.get_future().get();

    if(value.empty()) {
        return;
    }

    // deserialize
    std::stringstream stream(std::string(value.begin(), value.end()));
    cereal::PortableBinaryInputArchive arc(stream);

    SavePos data;
    arc(data);

    this->position = data.position;
    this->angles = data.angles;

    // send message to client
    PlayerPositionInitial initial;
    initial.position = data.position;
    initial.angles = data.angles;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(initial);

    this->client->writePacket(kEndpointPlayerMovement, kPlayerPositionInitial, oStream.str());
}

/**
 * Sends our position to all players, except ourselves.
 */
void PlayerMovement::broadcastPosition() {
    if(!this->needsBroadcast) return;

    // get our ID
    const auto ourId = this->client->getClientId();
    if(!ourId) return;

    // build the position update message
    PlayerPositionBroadcast b;

    b.position = this->position;
    b.angles = this->angles;
    b.playerId = *ourId;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(b);

    const auto str = oStream.str();

    // iterate over all clients
    this->client->getListener()->forEach([&, ourId, str](auto &client) {
        // ignore unauthenticated clients, or this client
        auto clientId = client->getClientId();
        if(!clientId) return;
        else if(*clientId == *ourId) return;

        // send packet
        client->writePacket(kEndpointPlayerMovement, kPlayerPositionBroadcast, str);
    });

    // clean flags
    this->needsBroadcast = false;
}
