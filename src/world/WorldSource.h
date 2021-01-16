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
#include <optional>
#include <unordered_map>
#include <mutex>

#include <uuid.h>
#include <glm/vec2.hpp>
#include <glm/gtx/hash.hpp>
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

        /// Reads the value of a world info key.
        std::promise<std::vector<char>> getWorldInfo(const std::string &key);

        /// Sets whether we ignore the file and generate all data
        void setGenerateOnly(const bool value) {
            this->generateOnly = value;
        }

        /// Blocks on writing all dirty blocks out to disk
        void flushDirtyChunksSync();

        /// Start of frame; used for deciding which chunks to write out
        void startOfFrame();

        /// Marks the given chunk as dirty.
        void markChunkDirty(std::shared_ptr<Chunk> &chunk);
        /// Forces a chunk to be written out. Will wait for this to complete
        void forceChunkWriteSync(std::shared_ptr<Chunk> &chunk);
        /// Forces a chunk to be written out, if it's dirty. Will wait for this to complete
        void forceChunkWriteIfDirtySync(std::shared_ptr<Chunk> &chunk);

        /// Gets the number of pending chunks to write (e.g. those that are dirty)
        const size_t numPendingWrites() const {
            return this->dirtyChunks.size();
            // return this->writeQueue.size_approx();
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

        void writerMain();

    private:
        /// Number of frames a chunk must be dirty before it's written out
        constexpr static const size_t kDirtyThreshold = 60*2.5;
        /// Maximum age of a write request before we force writing
        constexpr static const size_t kMaxWriteRequestAge = 60*30;

        /// Maximum number of chunks to queue for writing per frame
        constexpr static const size_t kMaxWriteChunksPerFrame = 2;

    private:
        struct DirtyChunkInfo {
            /// Chunk to write out
            std::shared_ptr<Chunk> chunk = nullptr;

            /// Frames since the chunk was last marked as dirty
            size_t framesSinceDirty = 0;
            /// Number of times the dirty frames counter was reset
            size_t numDirtyCounterResets = 0;
            /// Total frames this chunk has been waiting to be written out
            size_t totalFramesWaiting = 0;
        };

        struct WriteRequest {
            WriteRequest() {}
            WriteRequest(std::shared_ptr<Chunk> _chunk) : chunk(_chunk) {}

            std::shared_ptr<Chunk> chunk = nullptr;
            std::optional<std::function<void(void)>> completion;
        };

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

        /// chunk writer thread
        std::unique_ptr<std::thread> writerThread;
        /// write requests
        moodycamel::BlockingConcurrentQueue<WriteRequest> writeQueue;
        /// dirty chunks to be written out
        std::unordered_map<glm::ivec2, DirtyChunkInfo> dirtyChunks;
        /// lock protecting the dirty chunks map
        std::mutex dirtyChunksLock;

        /// when set, we accept work items
        std::atomic_bool acceptRequests;

        /// when set, we go directly to the generator for all chunks
        std::atomic_bool generateOnly;

        /// when set, we don't mess with the dirty chunks list
        std::atomic_bool inhibitDirtyChunkHandling = false;

        /// UUID of the current player
        uuids::uuid playerId;
};
}

#endif
