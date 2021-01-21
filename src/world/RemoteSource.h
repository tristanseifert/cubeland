#ifndef WORLD_REMOTESOURCE_H
#define WORLD_REMOTESOURCE_H

#include "ClientWorldSource.h"
#include <world/WorldSource.h>

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

        /// executes a function on the work queue, resulting a future holding its return value
        template<class F, class... Args>
        auto work(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
            // build a task from the function invocation
            using return_type = typename std::invoke_result<F, Args...>::type;
            auto task = std::make_shared< std::packaged_task<return_type()> >(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            // get future
            std::future<return_type> fut = task->get_future();

            // insert to queue
            if(!this->acceptRequests) {
                throw std::runtime_error("work queue not accepting requests");
            }
            this->workQueue.enqueue([task](){ (*task)(); });

            return fut;
        }
        void workerMain(size_t i);

    private:
        std::shared_ptr<net::ServerConnection> server = nullptr;

        const size_t numWorkers;
        std::atomic_bool workerRun;
        std::atomic_bool acceptRequests = true;

        /// worker threads
        std::vector<std::unique_ptr<std::thread>> workers;
        /// work requests sent to the thread
        moodycamel::BlockingConcurrentQueue<WorkItem> workQueue;
};
}
#endif
