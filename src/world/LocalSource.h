#ifndef WORLD_LOCALSOURCE_H
#define WORLD_LOCALSOURCE_H

#include "ClientWorldSource.h"
#include <world/WorldSource.h>

#include <memory>

namespace world {
/**
 * Wraps a generator and world reader for local use.
 */
class LocalSource: public ClientWorldSource, public WorldSource {
    public:
        LocalSource(std::shared_ptr<WorldReader> reader,
                std::shared_ptr<WorldGenerator> generator, const uuids::uuid &playerId,
                const size_t numThreads = 0) : ClientWorldSource(playerId),
    WorldSource(reader, generator, numThreads) {}

        /// Gets a chunk
        std::future<std::shared_ptr<Chunk>> getChunk(int x, int z) override {
            return WorldSource::getChunk(x, z);
        }

        std::future<void> setPlayerInfo(const uuids::uuid &id, const std::string &key, const std::vector<char> &value) override {
            return WorldSource::setPlayerInfo(id, key, value);
        }
        std::promise<std::vector<char>> getPlayerInfo(const uuids::uuid &id, const std::string &key) override {
            return WorldSource::getPlayerInfo(id, key);
        }
        std::promise<std::vector<char>> getWorldInfo(const std::string &key) override {
            return WorldSource::getWorldInfo(key);
        }

        /// TODO: implement properly
        std::promise<std::pair<glm::vec3, glm::vec3>> getInitialPosition() override {
            std::promise<std::pair<glm::vec3, glm::vec3>> prom;
            prom.set_exception(std::make_exception_ptr(std::runtime_error("unimplemented")));
            return prom;
        }
        /// TODO: implement properly
        std::promise<std::pair<glm::vec3, glm::vec3>> getSpawnPosition() override {
            std::promise<std::pair<glm::vec3, glm::vec3>> prom;
            prom.set_value(std::make_pair(glm::vec3(64), glm::vec3(0)));
            return prom;
        }

        /// Updates the dirty chunks list every frame
        void startOfFrame() override {
            ClientWorldSource::startOfFrame();
            this->updateDirtyList();
        }

        /// Requests writing out of all chunks; th
        void flushDirtyChunksSync() override {
            WorldSource::flushDirtyChunksSync();
        }

        void markChunkDirty(std::shared_ptr<Chunk> &chunk) override {
            WorldSource::markChunkDirty(chunk);
        }
        void forceChunkWriteIfDirtySync(std::shared_ptr<Chunk> &chunk) override {
            WorldSource::forceChunkWriteIfDirtySync(chunk);
        }

        const size_t numPendingWrites() const override {
            return WorldSource::numPendingWrites();
        }

        const bool isSinglePlayer() const override {
            return true;
        }
};
}

#endif
