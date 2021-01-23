#ifndef WORLD_CLIENTWORLDSOURCE_H
#define WORLD_CLIENTWORLDSOURCE_H

#include <world/AbstractWorldSource.h>

#include <chrono>
#include <future>
#include <optional>
#include <utility>
#include <string>

#include <glm/vec3.hpp>
#include <uuid.h>

namespace render {
class WorldRenderer;
class WorldRendererDebugger;
}

namespace world {
/**
 * Helpers for world sources operating on the client app.
 */
class ClientWorldSource: public AbstractWorldSource {
    friend class render::WorldRendererDebugger;
    friend class render::WorldRenderer;

    public:
        ClientWorldSource(const uuids::uuid &_playerId) : playerId(_playerId) {
            this->lastFrame = std::chrono::steady_clock::now();
        }
        virtual ~ClientWorldSource() = default;

        /// cancels any remaining work that's outstanding
        virtual void shutDown() {};

        using AbstractWorldSource::setPlayerInfo;
        /// Set the value of a player info key.
        std::future<void> setPlayerInfo(const std::string &key, const std::vector<char> &value) {
            return this->setPlayerInfo(this->playerId, key, value);
        }

        using AbstractWorldSource::getPlayerInfo;
        /// Reads the value of a player info key.
        std::promise<std::vector<char>> getPlayerInfo(const std::string &key) {
            return this->getPlayerInfo(this->playerId, key);
        }

        /// returns the position and view angles the player should load into the level at
        virtual std::promise<std::pair<glm::vec3, glm::vec3>> getInitialPosition() = 0;
        /// get the spawn position of the player, e.g. where they appear after dying
        virtual std::promise<std::pair<glm::vec3, glm::vec3>> getSpawnPosition() = 0;

        /// player position changed; by default this does nothing
        virtual void playerMoved(const glm::vec3 &pos, const glm::vec3 &angle) {};

        /// sets the pause flag (when set, we don't update the thyme)
        virtual void setPaused(const bool paused) {
            this->paused = paused;
            if(!paused) {
                this->lastFrame = std::chrono::steady_clock::now();
            }
        }
        /// gets the current time
        virtual const double getTime() const {
            return this->currentTime;
        }
        /// sets the current time
        virtual const void setTime(const double newTime) {
            this->currentTime = newTime;
        }
        /// set the time scale factor
        virtual void setTimeFactor(const double newFactor) {
            this->timeFactor = newFactor;
        }

        /// updates the world time
        virtual void startOfFrame() {
            using namespace std::chrono;

            if(!this->paused) {
                const auto diffUs = duration_cast<microseconds>(steady_clock::now() - this->lastFrame).count();
                this->currentTime += (((double) diffUs) / 1000. / 1000.) * this->timeFactor;
            }
            this->lastFrame = steady_clock::now();
        };

        /// Marks the given chunk as dirty.
        virtual void markChunkDirty(std::shared_ptr<Chunk> &chunk) = 0;
        /**
         * Force a chunk to be written if dirty. This will get called when the chunk is
         * unloaded, usually because it's out of the player's view.
         */
        virtual void forceChunkWriteIfDirtySync(std::shared_ptr<Chunk> &chunk) = 0;

        /// Returns the number of pending writes
        virtual const size_t numPendingWrites() const = 0;

        /// whether the world source is single player
        virtual const bool isSinglePlayer() const = 0;

        /// whether the world source is valid
        virtual const bool isValid() const {
            return this->valid;
        }
        /// error to display when world source becomes invalid
        virtual std::optional<std::string> getErrorStr() const {
            return std::nullopt;
        }

    protected:
        uuids::uuid playerId;
        bool valid = true;

        /// pause flag; inhibits the incrementing of time
        bool paused = false;
        /// conversion from wall clock seconds to world thyme
        double timeFactor = 1. / (60. * 24.);
        double currentTime = 0;
        std::chrono::steady_clock::time_point lastFrame;
};
}

#endif
