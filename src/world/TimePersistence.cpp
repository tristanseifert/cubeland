#include "TimePersistence.h"
#include "WorldSource.h"

#include "world/tick/TickHandler.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <vector>
#include <sstream>

using namespace world;

const std::string TimePersistence::kDataPlayerInfoKey = "world.time";

/**
 * Sets up the time persistence handler, and installs the tick callback to periodically save the
 * current time.
 *
 * Note that we'll try to restore the time here.
 */
TimePersistence::TimePersistence(std::shared_ptr<WorldSource> &_s, double *_t) : source(_s),
    time(_t) {
    // try to restore time
    this->load();

    // add the tick handler
    this->tickCallback = TickHandler::add(std::bind(&TimePersistence::tick, this));
}

/**
 * Removes our tick callback.
 *
 * Note we don't bother writing the time here since it's not super critical.
 */
TimePersistence::~TimePersistence() {
    if(this->tickCallback) {
        TickHandler::remove(this->tickCallback);
    }
}



/**
 * Tick callback; this periodically saves the time back to the world file.
 */
void TimePersistence::tick() {
    // bail if not beyond max ticks
    if(this->ticksSinceSave++ < kSaveInterval) {
        return;
    }

    // serialize the time
    this->save();

    // reset flags
    this->ticksSinceSave = 0;
}

/**
 * Restores the time from the world file.
 *
 * @return Whether world time was updated or not
 */
bool TimePersistence::load() {
    PROFILE_SCOPE(WorldTimeLoad);

    // see if we can get the data from the world source
    auto promise = this->source->getPlayerInfo(kDataPlayerInfoKey);
    auto value = promise.get_future().get();

    if(value.empty()) {
        return false;
    }

    try {
        // try to decode position data
        std::stringstream stream(std::string(value.begin(), value.end()));
        cereal::PortableBinaryInputArchive arc(stream);

        TimeInfo data;
        arc(data);

        // copy it out
        *this->time = data.time;
        return true;
    } catch(std::exception &e) {
        Logging::error("Failed to restore world time: {}", e.what());
        return false;
    }
}

/**
 * Serializes time and writes it to the world file.
 */
void TimePersistence::save() {
    PROFILE_SCOPE(WorldTimeSave);

    // build time struct
    TimeInfo i;
    i.time = *this->time;

    // serialize the time struct
    std::stringstream stream;
    cereal::PortableBinaryOutputArchive arc(stream);
    arc(i);

    const auto str = stream.str();
    std::vector<char> rawBytes(str.begin(), str.end());

    // write it out to the world file
    auto future = this->source->setPlayerInfo(kDataPlayerInfoKey, rawBytes);
    future.wait();
}
