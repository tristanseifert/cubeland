#include "PlayerInfo.h"
#include "net/ServerConnection.h"

#include <net/PacketTypes.h>
#include <net/EPPlayerInfo.h>

#include <Logging.h>
#include <io/Format.h>

#include <cereal/archives/portable_binary.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Initializes the player info handler.
 */
PlayerInfo::PlayerInfo(ServerConnection *_server) : PacketHandler(_server) {

}

/**
 * Notify any pending waits that we're exiting.
 */
PlayerInfo::~PlayerInfo() {
    std::lock_guard<std::mutex> lg(this->requestsLock);
    for(auto &[key, promise] : this->requests) {
        promise.set_value(std::nullopt);
    }
}



/**
 * We handle all player info endpoint packets.
 */
bool PlayerInfo::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointPlayerInfo) return false;
    // it musn't be more than the max value
    if(header.type >= kPlayerInfoTypeMax) return false;

    return true;
}


/**
 * Handles player info endpoint packets. This should be really just the read request responses, or
 * acks for writes.
 */
void PlayerInfo::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    switch(header.type) {
        case kPlayerInfoGetResponse:
            this->receivedKey(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid player info packet type: ${:02x}", header.type));
    }
}



/**
 * Sends a request to the server to get a particular player info key.
 */
std::future<std::optional<std::vector<std::byte>>> PlayerInfo::get(const std::string &key) {
    // set up the promise and save it
    std::promise<std::optional<std::vector<std::byte>>> prom;
    auto future = prom.get_future();

    {
        std::lock_guard<std::mutex> lg(this->requestsLock);
        XASSERT(!this->requests.contains(key), "Already waiting for player info read for key {}!", key);
        this->requests[key] = std::move(prom);
    }

    // build the request
    PlayerInfoGet request(key);

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(request);

    // send it
    this->server->writePacket(kEndpointPlayerInfo, kPlayerInfoGet, oStream.str());
    return future;
}


/**
 * A response to a previous "get player info" request has been received.
 *
 * Parse the response, and set the value on any relevant futures we've got stored.
 */
void PlayerInfo::receivedKey(const PacketHeader &hdr, const void *payload, const size_t payloadLen) {
    // deserialize the respone
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    PlayerInfoGetReply response;
    iArc(response);

    // complete the appropriate promise
    std::lock_guard<std::mutex> lg(this->requestsLock);

    auto &prom = this->requests.at(response.key);

    try {
        if(!response.found) {
            prom.set_value(std::nullopt);
        } else {
            prom.set_value(*response.data);
        }
    } catch(std::exception &e) {
        Logging::error("Failed to handle received player info key: {}", e.what());
        prom.set_exception(std::current_exception());
    }

    // remove from list
    this->requests.erase(response.key);
}



/**
 * Builds ands sends a packet to set a player info key.
 */
void PlayerInfo::set(const std::string &key, const std::vector<std::byte> &value) {
    // build the request
    PlayerInfoSet request;
    request.key = key;
    request.data = value;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(request);

    // send it
    this->server->writePacket(kEndpointPlayerInfo, kPlayerInfoSet, oStream.str());
}

