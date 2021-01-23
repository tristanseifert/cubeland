#include "Time.h"
#include "net/ListenerClient.h"
#include "net/Listener.h"

#include "world/time/Clock.h"

#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPTime.h>
#include <io/ConfigManager.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/// number of instances of the time handler we've got
std::atomic_uint Time::numConnectedClients = 0;

/// this timer is used for both advancing the time step, but also for updating clients
CppTime::Timer Time::timer;

/**
 * Sets up a new time handler. This also increments the connected client count and starts the
 * thymer.
 */
Time::Time(ListenerClient *_client) : PacketHandler(_client) {
    // start time updates if needed
    if(++numConnectedClients == 1) {
        auto clock = this->client->getListener()->getClock();
        clock->resume();
    }

    // install the client update timer
    const auto updateFreq = io::ConfigManager::getUnsigned("proto.timeUpdateInterval", 10);
    const auto interval = std::chrono::seconds(updateFreq);

    this->updateTimer = timer.add(interval, [&](auto) {
        this->sendTime();
    }, interval);
}

/**
 * Discards the time handler. If no other time handlers remain (because no players are connected
 * to the server) we'll stop the time.
 */
Time::~Time() {
    // stop time updates if needed
    if(--numConnectedClients == 0) {
        auto clock = this->client->getListener()->getClock();
        clock->stop();
    }

    // remove client update timer
    timer.remove(this->updateTimer);
}

/**
 * We handle all time endpoint packets.
 */
bool Time::canHandlePacket(const PacketHeader &header) {
    // it must be the world info endpoint
    if(header.endpoint != kEndpointTime) return false;
    // it musn't be more than the max value
    if(header.type >= kTimeTypeMax) return false;

    return true;
}

/**
 * Handles time packets
 */
void Time::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        // no client to server packets defined
        default:
            throw std::runtime_error(f("Invalid time packet type: ${:02x}", header.type));
    }
}

/**
 * Send the current time when auth state changed.
 */
void Time::authStateChanged() {
    if(!this->client->getClientId()) return;

    // build initial structyboi
    const auto secsPerDay = io::ConfigManager::getUnsigned("proto.secsPerDay", 60*24);

    TimeInitialState init;
    init.tickFactor = 1. / ((double) secsPerDay);
    init.currentTime = this->client->getListener()->getClock()->getTime();

    // serialize and send
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(init);

    this->client->writePacket(kEndpointTime, kTimeInitialState, oStream.str());
}

/**
 * Sends a time update.
 */
void Time::sendTime() {
    TimeUpdate update;
    update.currentTime = this->client->getListener()->getClock()->getTime();

    // serialize and send
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(update);

    this->client->writePacket(kEndpointTime, kTimeUpdate, oStream.str());
}

