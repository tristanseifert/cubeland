/**
 * Most chunk updates and other processing run on worker threads shared between all of the chunks
 * on screen, basically a specialized thread pool.
 */
#ifndef RENDER_CHUNK_CHUNKWORKER_H
#define RENDER_CHUNK_CHUNKWORKER_H

#include "util/ThreadPool.h"

#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <future>

#include <blockingconcurrentqueue.h>

namespace render::chunk {
using WorkItem = std::function<void(void)>;
class ChunkWorker: public util::ThreadPool<WorkItem> {
    public:
        // you should not call these
        ChunkWorker();
        ~ChunkWorker();

        // instead, use these!
        /// Gets a reference to the chunk workers
        /*static std::shared_ptr<ChunkWorker> shared() {
            return gShared;
        }*/

        /// Pushes a work request with a more substantive return type
        template <typename ...Params>
        static auto pushWork(Params&&... params) 
        -> std::future<void> {
            return gShared->queueWorkItem(std::forward<Params>(params)...);
        }
        /// number of pending work items
        static size_t getPendingItemCount() {
            return gShared->numPending();
        }
        /// whether we can have more than one work thread
        static bool hasMultipleWorkers() {
            return gShared->getNumWorkers() > 1;
        }

        /// Forces initialization of the chunk worker threads
        static void init() {
            gShared = std::make_unique<ChunkWorker>();
        }
        /// Releases the shared reference, in turn shutting down the workers
        static void shutdown() {
            gShared = nullptr;
        }

    private:
        /// enqueue an empty work item
        void pushNop() {
            this->workQueue.enqueue([&] {});
        }

        void workerThreadStarted(const size_t i) override;

    private:
        static std::shared_ptr<ChunkWorker> gShared;

    private:
};
}

#endif
