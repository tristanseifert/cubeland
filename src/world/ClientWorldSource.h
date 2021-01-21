#ifndef WORLD_CLIENTWORLDSOURCE_H
#define WORLD_CLIENTWORLDSOURCE_H

#include <world/AbstractWorldSource.h>

#include <uuid.h>

namespace world {
/**
 * Helpers for world sources operating on the client app.
 */
class ClientWorldSource: public AbstractWorldSource {
    public:
        ClientWorldSource(const uuids::uuid &_playerId) : playerId(_playerId) {

        }
        virtual ~ClientWorldSource() = default;

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

        /// Perform some internal housekeeping
        virtual void startOfFrame() = 0;

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

    protected:
        uuids::uuid playerId;
};
}

#endif
