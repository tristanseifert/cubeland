/**
 * Most chunk updates and other processing run on worker threads shared between all of the chunks
 * on screen, basically a specialized thread pool.
 */
#ifndef RENDER_CHUNK_CHUNKWORKER_H
#define RENDER_CHUNK_CHUNKWORKER_H

#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <future>

#include <blockingconcurrentqueue.h>

namespace render::chunk {
class ChunkWorker {
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
        template<class F, class... Args>
        static auto pushWork(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
            // build a task from the function invocation
            using return_type = typename std::invoke_result<F, Args...>::type;
            auto task = std::make_shared< std::packaged_task<return_type()> >(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            // get future
            std::future<return_type> fut = task->get_future();

            // insert to queue
            if(!gShared->acceptRequests) {
                throw std::runtime_error("work queue not accepting requests");
            }
            gShared->workQueue.enqueue([task](){ (*task)(); });

            return fut;
        }

        /// Pushes a work request
        /*static std::promise<void> pushWork(const std::function<void(void)> &f) {
            std::promise<void> prom;
            gShared->workQueue.enqueue([&, f]() mutable {
                try {
                    f();
                    prom.set_value();
                } catch(std::exception &) {
                    prom.set_exception(std::current_exception());
                }
            });
            return prom;
        }*/

        /// whether we can have more than one work thread
        static bool hasMultipleWorkers() {
            return (gShared->numWorkers > 1);
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

        void workerMain(size_t i);

    private:
        static std::shared_ptr<ChunkWorker> gShared;

    private:
        using WorkItem = std::function<void(void)>;

        /// number of worker threads to create
        size_t numWorkers;
        /// worker thread processes requests as long as this is set
        std::atomic_bool workerRun;
        /// worker threads
        std::vector<std::unique_ptr<std::thread>> workers;
        /// work requests sent to the thread
        moodycamel::BlockingConcurrentQueue<WorkItem> workQueue;

        /// when set, we accept work items
        std::atomic_bool acceptRequests;
};
}

#endif
