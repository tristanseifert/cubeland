#include "Clock.h"

#include <io/ConfigManager.h>
#include <world/WorldSource.h>

#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>

using namespace world;

const std::string Clock::kTimeInfoKey = "server.world.time";

/**
 * Sets up the clock worker.
 */
Clock::Clock(WorldSource *_source) : source(_source) {
    // calculate the tick step
    const auto secsPerDay = io::ConfigManager::getUnsigned("proto.secsPerDay", 60*24);
    this->tickStep = 1. / ((double) secsPerDay);

    // load initial time
    this->loadTime();
}

/**
 * Save time out and remove timer when deleting.
 */
Clock::~Clock() {
    if(!this->isPaused) {
        this->stop();
    }

    this->saveTime();
}

/**
 * Starts updating the clock.
 */
void Clock::resume() {
    XASSERT(this->isPaused, "Cannot resume an already running clock");

    const auto interval = std::chrono::milliseconds(kUpdateInterval);

    this->isPaused = false;
    this->lastStep = std::chrono::steady_clock::now();
    this->updateTimer = this->timer.add(interval, [&](auto) {
        this->step();
    }, interval);
}

/**
 * Stops the clock.
 */
void Clock::stop() {
    XASSERT(!this->isPaused, "Cannot stop an already stopped clock");

    this->timer.remove(this->updateTimer);
    this->isPaused = true;

    this->saveTime();
}

/**
 * Updates the current time.
 */
void Clock::step() {
    using namespace std::chrono;

    const auto now = steady_clock::now();
    const auto diffUs = duration_cast<microseconds>(now - this->lastStep);
    const auto diffSec = ((double) diffUs.count()) / 1000. / 1000.;

    this->currentTime += (this->tickStep * diffSec);
    this->lastStep = now;
}

/**
 * Saves the current time into the world info.
 */
void Clock::saveTime() {
    // serialize the time
    TimeData data;

    data.time = this->currentTime;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(data);

    // save world info
    const auto &str = oStream.str();
    std::vector<char> bytes(str.begin(), str.end());

    auto promise = this->source->setWorldInfo(kTimeInfoKey, bytes);
    promise.get_future().get();
}

/**
 * Loads time from world info. 
 */
void Clock::loadTime() {
    // try to read the world info
    auto infoProm = this->source->getWorldInfo(kTimeInfoKey);
    auto infoFuture = infoProm.get_future();
    auto value = infoFuture.get();

    if(value.empty()) {
        return;
    }

    // deserialize it
    std::stringstream stream(std::string(value.begin(), value.end()));
    cereal::PortableBinaryInputArchive arc(stream);

    TimeData data;
    arc(data);

    this->currentTime = data.time;
}
