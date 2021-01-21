#include "PlayerInfo.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPPlayerInfo.h>

#include <io/Format.h>

#include <cereal/archives/portable_binary.hpp>

#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Initializes the authentication handler
 */
PlayerInfo::PlayerInfo(ListenerClient *_client) : PacketHandler(_client) {
}

/**
 * We handle all world info endpoint packets.
 */
bool PlayerInfo::canHandlePacket(const PacketHeader &header) {
    // it must be the world info endpoint
    if(header.endpoint != kEndpointPlayerInfo) return false;
    // it musn't be more than the max value
    if(header.type >= kPlayerInfoTypeMax) return false;

    return true;
}

/**
 * Handles world info packets
 */
void PlayerInfo::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        case kPlayerInfoGet:
            this->handleGet(header, payload, payloadLen);
            break;
        case kPlayerInfoSet:
            this->handleSet(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid player info packet type: ${:02x}", header.type));
    }
}

/**
 * Handles reading the world info key.
 */
void PlayerInfo::handleGet(const PacketHeader &hdr, const void *payload, const size_t payloadLen) {
    auto world = this->client->getWorld();
    auto playerId = *this->client->getClientId();

    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    PlayerInfoGet request;
    iArc(request);

    // do it
    auto info = world->getPlayerInfo(playerId, request.key);
    auto value = info.get_future().get();

    // build response
    PlayerInfoGetReply reply;

    reply.key = request.key;
    reply.found = !value.empty();
    if(reply.found) {
        // XXX: this sucks. we should refactor world source to use std::byte
        std::vector<std::byte> temp;
        temp.resize(value.size());
        memcpy(temp.data(), value.data(), value.size());

        reply.data = temp;
    }

    // send it
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(reply);

    this->client->writePacket(kEndpointPlayerInfo, kPlayerInfoGetResponse, oStream.str(), hdr.tag);
}



/**
 * Handles setting a player info key.
 */
void PlayerInfo::handleSet(const PacketHeader &hdr, const void *payload, const size_t payloadLen) {
    auto world = this->client->getWorld();
    auto playerId = *this->client->getClientId();

    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    PlayerInfoSet request;
    iArc(request);

    // perform the setting
    std::vector<char> data;

    if(request.data) {
        data.resize(request.data->size());
        memcpy(data.data(), request.data->data(), request.data->size());
    }

    auto fut = world->setPlayerInfo(playerId, request.key, data);
    fut.get();
}
