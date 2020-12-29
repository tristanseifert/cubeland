#include "PlayerPosPersistence.h"
#include "InputManager.h"

#include "world/WorldSource.h"
#include "world/tick/TickHandler.h"
#include "io/Serialization.h"
#include "io/Format.h"
#include <Logging.h>

#include <glm/gtc/epsilon.hpp>
#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <sstream>

using namespace input;

const std::string PlayerPosPersistence::kDataPlayerInfoKey = "player.position";

/**
 * Sets up the position persistence. This sets up our tick handler, where we repeatedly check
 * whether the position or view angles changed and force them to be saved.
 */
PlayerPosPersistence::PlayerPosPersistence(InputManager *_m,
        std::shared_ptr<world::WorldSource> &_s) : input(_m), source(_s) {
    // install tick handler
    this->dirty = false;
    this->tickHandler = world::TickHandler::add(std::bind(&PlayerPosPersistence::tick, this));
}

/**
 * Forces the position to be saved, if needed, and removes our tick callback.
 */
PlayerPosPersistence::~PlayerPosPersistence() {
    // remove timer
    world::TickHandler::remove(this->tickHandler);
    this->tickHandler = 0;

    // save if needed
    if(this->dirty) {
        this->writePosition();
    }
}

/**
 * Yeets the current position and compares it against the stored value.
 */
void PlayerPosPersistence::startOfFrame(const glm::vec3 &currentPos) {
    bool changed = false;

    // see if the camera angles changed significantly
    const glm::vec2 currentAngles(this->input->pitch, this->input->yaw);
    if(glm::any(glm::epsilonNotEqual(currentAngles, this->lastAngles, kAngleEpsilon))) {
        changed = true;
        goto beach;
    }

    // see if the position changed significantly
    if(glm::any(glm::epsilonNotEqual(currentPos, this->lastPosition, kPositionEpsilon))) {
        changed = true;
        goto beach;
    }

beach:;
    // update cached values and mark as dirty
    if(!changed) return;

    this->lastPosition = currentPos;
    this->lastAngles = currentAngles;

    this->dirty = true;
}

/**
 * Increments the dirty tick counter until it reaches the limit, at which point the current
 * position is written out to the file.
 */
void PlayerPosPersistence::tick() {
    PROFILE_SCOPE(PlayerPosPersistence);

    // bail if not dirty; handle the counter otherwise
    if(!this->dirty) return;
    if(this->dirtyTicks++ < kSaveDelayTicks) return;

    // write it out and reset dirty flag
    this->saveWorker.queueWorkItem([&]{
        this->writePosition();
    });

    this->dirty = false;
    this->dirtyTicks = 0;
}



/**
 * Attempts to load the world position and camera look angles from the player info stored in the
 * world data. Note that this takes place synchronously.
 */
bool PlayerPosPersistence::loadPosition(glm::vec3 &loadedPos) {
    // try to load the inventory data
    auto promise = this->source->getPlayerInfo(kDataPlayerInfoKey);
    auto value = promise.get_future().get();

    if(value.empty()) {
        return false;
    }

    // try to decode position data
    std::stringstream stream(std::string(value.begin(), value.end()));
    cereal::PortableBinaryInputArchive arc(stream);

    PlayerPosData data;
    arc(data);

    // copy it out
    this->lastAngles = data.cameraAngles;
    this->lastPosition = data.position;

    loadedPos = data.position;
    this->input->pitch = data.cameraAngles.x;
    this->input->yaw = data.cameraAngles.y;

    this->dirty = false;
    return true;
}

/**
 * Serializes a struct containing the current world position and camera look angles; this is then
 * stored as a player info key.
 */
void PlayerPosPersistence::writePosition() {
    // build the position and look angles data
    PlayerPosData data;

    data.position = this->lastPosition;
    data.cameraAngles = this->lastAngles;

    Logging::trace("Saving position {} angles {}", data.position, data.cameraAngles);

    // serialize and compress it
    std::stringstream stream;
    cereal::PortableBinaryOutputArchive arc(stream);
    arc(data);

    const auto str = stream.str();
    std::vector<char> rawBytes(str.begin(), str.end());

    // lastly, write it and clear dirty flag
    auto future = this->source->setPlayerInfo(kDataPlayerInfoKey, rawBytes);
    future.wait();
}

