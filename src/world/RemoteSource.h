#ifndef WORLD_REMOTESOURCE_H
#define WORLD_REMOTESOURCE_H

#include "ClientWorldSource.h"
#include <world/WorldSource.h>
#include <util/ThreadPool.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace net {
class ServerConnection;
}

namespace world {
/**
 * A small wrapper around the raw server connection to enable getting chunks and all that fun
 * stuff.
 */
class RemoteSource: public ClientWorldSource {
    public:
        RemoteSource(std::shared_ptr<net::ServerConnection> conn, const uuids::uuid &playerId,
                const size_t numThreads);
        virtual ~RemoteSource();

        /// Gets a chunk
        std::future<std::shared_ptr<Chunk>> getChunk(int x, int z) override;

        std::future<void> setPlayerInfo(const uuids::uuid &id, const std::string &key, const std::vector<char> &value) override;
        std::promise<std::vector<char>> getPlayerInfo(const uuids::uuid &id, const std::string &key) override;
        std::promise<std::vector<char>> getWorldInfo(const std::string &key) override;

        /// Start of frame handler
        void startOfFrame() override;

        /// Requests writing out of all chunks; for us this just drains the send queue
        void flushDirtyChunksSync() override;

        /// marking chunks dirty is not supported; we stream block updates
        void markChunkDirty(std::shared_ptr<Chunk> &chunk) override {}
        /**
         * Ignore requests to force write dirty chunks, since chunks can't be marked dirty with the
         * server protocol. This is because any changes are sent on the block level.
         */
        void forceChunkWriteIfDirtySync(std::shared_ptr<Chunk> &chunk) override;

        /// number of block updates to process
        const size_t numPendingWrites() const override {
            return 0;
        }
        const bool isSinglePlayer() const override {
            return false;
        }

    private:
        using WorkItem = std::function<void(void)>;

    private:
        std::shared_ptr<net::ServerConnection> server = nullptr;

        const size_t numWorkers;
        std::atomic_bool acceptRequests = true;

        /// thread pool
        util::ThreadPool<WorkItem> *pool = nullptr;
};
}
#endif
