/**
 * World sources combine a generator and reader to provide an unified interface to world data.
 *
 * This in effect allows for the idea of sparse worlds that are generated on demand; the disk file
 * could store only changed chunks, for example. More data is generated automatically as the
 * player travels to outer edges of the world.
 */
#ifndef WORLD_WORLDSOURCE_H
#define WORLD_WORLDSOURCE_H

#include <memory>
#include <cstddef>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>
#include <future>

#include <uuid.h>
#include <blockingconcurrentqueue.h>

namespace world {
struct Chunk;

class WorldReader;
class WorldGenerator;

class WorldSource {
    public:
        WorldSource(std::shared_ptr<WorldReader> reader,
                std::shared_ptr<WorldGenerator> generator, const size_t numThreads = 0);
        virtual ~WorldSource();

        /// Gets a chunk from either the file or the world generator.
        std::future<std::shared_ptr<Chunk>> getChunk(int x, int z) {
            return this->work([&, x, z]{
                return this->workerGetChunk(x, z);
            });
        }

        /// Set the value of a player info key.
        std::future<void> setPlayerInfo(const std::string &key, const std::vector<char> &value);
        /// Reads the value of a player info key.
        std::promise<std::vector<char>> getPlayerInfo(const std::string &key);

        /// Sets whether we ignore the file and generate all data
        void setGenerateOnly(const bool value) {
            this->generateOnly = value;
        }

    private:
        using WorkItem = std::function<void(void)>;

        std::shared_ptr<Chunk> workerGetChunk(const int x, const int z);

    private:
        // executes a function on the work queue, resulting a future holding its return value
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

        /// enqueue an empty work item
        void pushNop() {
            this->workQueue.enqueue([&] {});
        }
        void workerMain(size_t i);

    private:
        // file is the primary backing store
        std::shared_ptr<WorldReader> reader = nullptr;
        // generator to fill in areas not backed by the file
        std::shared_ptr<WorldGenerator> generator = nullptr;

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

        /// when set, we go directly to the generator for all chunks
        std::atomic_bool generateOnly;

        /// UUID of the current player
        uuids::uuid playerId;
};
}

#endif
