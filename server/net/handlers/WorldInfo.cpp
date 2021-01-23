#include "WorldInfo.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPWorldInfo.h>

#include <io/Format.h>

#include <cereal/archives/portable_binary.hpp>

#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Initializes the world info handler
 */
WorldInfo::WorldInfo(ListenerClient *_client) : PacketHandler(_client) {
}

/**
 * We handle all world info endpoint packets.
 */
bool WorldInfo::canHandlePacket(const PacketHeader &header) {
    // it must be the world info endpoint
    if(header.endpoint != kEndpointWorldInfo) return false;
    // it musn't be more than the max value
    if(header.type >= kWorldInfoTypeMax) return false;

    return true;
}

/**
 * Handles world info packets
 */
void WorldInfo::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        case kWorldInfoGet:
            this->handleGet(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid world info packet type: ${:02x}", header.type));
    }
}

/**
 * Handles reading the world info key.
 */
void WorldInfo::handleGet(const PacketHeader &hdr, const void *payload, const size_t payloadLen) {
    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    WorldInfoGet request;
    iArc(request);

    // send the key
    this->sendKey(request.key, hdr.tag);
}

/**
 * Sends the value of a key to the client. This is used both to push keys to the client's cache,
 * and to handle client-initiated gets.
 */
void WorldInfo::sendKey(const std::string &key, const uint16_t tag) {
    auto world = this->client->getWorld();

    // read the key
    auto info = world->getWorldInfo(key);
    auto value = info.get_future().get();

    // build response
    WorldInfoGetReply reply;

    reply.key = key;
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

    this->client->writePacket(kEndpointWorldInfo, kWorldInfoGetResponse, oStream.str(), tag);
}

/**
 * When we become authorized, push to the client the world id.
 */
void WorldInfo::authStateChanged() {
    if(!this->client->getClientId()) return;

    // list of keys to push to the client
    this->sendKey("world.id");
}
