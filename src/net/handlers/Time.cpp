#include "Time.h"
#include "net/ServerConnection.h"

#include <net/PacketTypes.h>
#include <net/EPTime.h>

#include <Logging.h>
#include <io/Format.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * We handle all time endpoint packets.
 */
bool Time::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointTime) return false;
    // it musn't be more than the max value
    if(header.type >= kTimeTypeMax) return false;

    return true;
}


/**
 * Handles world info packets.
 */
void Time::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    PROFILE_SCOPE(Time);

    switch(header.type) {
        case kTimeInitialState:
            this->configTime(header, payload, payloadLen);
            break;

        case kTimeUpdate:
            this->resyncTime(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid player movement packet type: ${:02x}", header.type));
    }
}



/**
 * Configures our local clock with the initial time and update frequency. These are properties of
 * the world renderer.
 */
void Time::configTime(const PacketHeader &, const void *payload, const size_t payloadLen) {
    // deserialize the payload
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    TimeInitialState init;
    iArc(init);

    // configure time
    this->timeFactor = init.tickFactor;

    Logging::trace("Current time: {}, step {}", init.currentTime, init.tickFactor);
}


/**
 * Resynchronizes local time with the server's time.
 */
void Time::resyncTime(const PacketHeader &, const void *payload, const size_t payloadLen) {
    // deserialize the payload
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    TimeUpdate update;
    iArc(update);

    // update time
    Logging::trace("Resync time: server = {}", update.currentTime);
}

