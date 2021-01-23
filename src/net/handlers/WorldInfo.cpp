#include "WorldInfo.h"
#include "net/ServerConnection.h"

#include <net/PacketTypes.h>
#include <net/EPWorldInfo.h>

#include <Logging.h>
#include <io/Format.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Initializes the player info handler.
 */
WorldInfo::WorldInfo(ServerConnection *_server) : PacketHandler(_server) {

}

/**
 * Notify any pending waits that we're exiting.
 */
WorldInfo::~WorldInfo() {
    std::lock_guard<std::mutex> lg(this->requestsLock);
    for(auto &[key, promise] : this->requests) {
        promise.set_exception(std::make_exception_ptr(std::runtime_error("WorldInfo deallocating")));
    }
}



/**
 * We handle all world info endpoint packets.
 */
bool WorldInfo::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointWorldInfo) return false;
    // it musn't be more than the max value
    if(header.type >= kWorldInfoTypeMax) return false;

    return true;
}


/**
 * Handles world info packets.
 */
void WorldInfo::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    PROFILE_SCOPE(WorldInfo);

    switch(header.type) {
        case kWorldInfoGetResponse:
            this->receivedKey(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid player info packet type: ${:02x}", header.type));
    }
}



/**
 * Sends a request to the server to get a particular world info key.
 */
std::future<std::optional<std::vector<std::byte>>> WorldInfo::get(const std::string &key) {
    // set up the promise and check value cache
    std::promise<std::optional<std::vector<std::byte>>> prom;
    auto future = prom.get_future();

    {
        std::lock_guard<std::mutex> lg(this->cacheLock);
        if(this->cache.contains(key)) {
            prom.set_value(this->cache[key]);
            return future;
        }
    }

    // not in cache, so save promise and build request
    {
        std::lock_guard<std::mutex> lg(this->requestsLock);
        XASSERT(!this->requests.contains(key), "Already waiting for world info read for key {}!", key);
        this->requests[key] = std::move(prom);
    }

    // build the request
    WorldInfoGet request(key);

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(request);

    // send it
    this->server->writePacket(kEndpointWorldInfo, kWorldInfoGet, oStream.str());
    return future;
}


/**
 * A response to a previous "get player info" request has been received.
 *
 * Parse the response, and set the value on any relevant futures we've got stored.
 */
void WorldInfo::receivedKey(const PacketHeader &hdr, const void *payload, const size_t payloadLen) {
    // deserialize the respone
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    WorldInfoGetReply response;
    iArc(response);

    // store in cache if not empty
    if(response.found && !response.data->empty()) {
        std::lock_guard<std::mutex> lg(this->cacheLock);
        this->cache[response.key] = *response.data;
    }

    // complete the appropriate promise (if it exists; if not, we got a key pushed to our cache)
    std::lock_guard<std::mutex> lg(this->requestsLock);

    if(this->requests.contains(response.key)) {
        auto &prom = this->requests.at(response.key);

        try {
            if(!response.found) {
                prom.set_value(std::nullopt);
            } else {
                prom.set_value(*response.data);
            }
        } catch(std::exception &e) {
            Logging::error("Failed to handle received world info key: {}", e.what());
            prom.set_exception(std::current_exception());
        }

        // remove from list
        this->requests.erase(response.key);
    }
}
