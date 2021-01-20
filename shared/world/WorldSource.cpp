#include "WorldSource.h"
#include "WorldReader.h"
#include "WorldGenerator.h"

#include "chunk/Chunk.h"
#include "chunk/ChunkSlice.h"
#include <io/Format.h>
#include <util/Thread.h>

#include <Logging.h>

#include <utility>
#include <chrono>
#include <algorithm>
#include <random>

#if PROFILE
#include <mutils/time/profiler.h>
#else
#define PROFILE_SCOPE(x) 
#define LOCK_GUARD(lock, name) std::lock_guard<std::mutex> lg(lock)
#endif

using namespace world;

/**
 * Sets up a world source.
 *
 * This spawns our background worker thread pool, which is where we'll synchronously execute all
 * generation/wait for it to complete. This means these are relatively heavy objects to allocate,
 * but that's ok since we'll really only have one of them.
 *
 * @param numThreads Number of worker threads to allocate; if 0, use the default value from the
 * user's preferences.
 */
WorldSource::WorldSource(std::shared_ptr<WorldReader> _r, std::shared_ptr<WorldGenerator> _g,
        const uuids::uuid &_id, const size_t _numThreads) : generator(_g), playerId(_id), reader(_r) {
    XASSERT(_numThreads, "Invalid thread count for world source");
    
    // set up some additional initial state
    this->generateOnly = false;
    this->acceptRequests = true;

    // set up other worker threads
    this->numWorkers = _numThreads;
    this->workerRun = true;

    for(size_t i = 0; i < this->numWorkers; i++) {
        auto worker = std::make_unique<std::thread>(&WorldSource::workerMain, this, i);
        this->workers.push_back(std::move(worker));
    }

    // set up the writer thread
    this->writerThread = std::make_unique<std::thread>(&WorldSource::writerMain, this);
}

/**
 * Ensures all work threads are shut down cleanly.
 */
WorldSource::~WorldSource() {
    this->flushDirtyChunksSync();

    // set stop flag and queue nThreads+1 NOPs
    this->acceptRequests = false;
    this->workerRun = false;

    this->writeQueue.enqueue(WriteRequest());

    for(size_t i = 0; i < this->numWorkers+1; i++) {
        this->pushNop();
    }

    // wait for all threads to join
    for(auto &thread : this->workers) {
        thread->join();
    }

    this->writerThread->join();
}

/**
 * Synchronously writes all chunks.
 */
void WorldSource::flushDirtyChunksSync() {
    this->inhibitDirtyChunkHandling = true;

    // force all chunks to finish writing
    if(!this->dirtyChunks.empty() && !this->generateOnly) {
        Logging::info("Waiting for {} dirty chunk(s) to finish writing", this->dirtyChunks.size());
        for(auto &[pos, info] : this->dirtyChunks) {
            this->forceChunkWriteSync(info.chunk);
        }
    }

    this->inhibitDirtyChunkHandling = false;
}

/**
 * Retrieves a chunk of the world.
 *
 * This will first check if the chunk exists in the persistent (WorldReader) backing store. If so,
 * it is read from there. Otherwise, we generate it on our background thread and return it.
 */
std::shared_ptr<Chunk> WorldSource::workerGetChunk(const int x, const int z) {
    // check the world reader if we're not in generate only mode
    if(!this->generateOnly && this->reader) {
        auto exists = this->reader->chunkExists(x, z);
        if(exists.get_future().get()) {
            auto chunk = this->reader->getChunk(x, z);
            auto future = chunk.get_future();
            return future.get();
        }
    }

    // if we get here, we need to invoke the generatour; also immediately mark it as dirty
    auto generated = this->generator->generateChunk(x, z);
    // this->markChunkDirty(generated);

    return generated;
}



/**
 * Main loop for the world source threads
 */
void WorldSource::workerMain(size_t i) {
    // perform some setup
    const auto threadName = f("WorldSource {}", i+1);
#if PROFILE
    MUtils::Profiler::NameThread(threadName.c_str());
#endif
    util::Thread::setName(threadName);

    // main loop; dequeue work items
    WorkItem item;
    while(this->workerRun) {
        this->workQueue.wait_dequeue(item);
        item();
    }

#if PROFILE
    MUtils::Profiler::FinishThread();
#endif
}

/**
 * Writes some player info data. This will go directly to the world reader.
 */
std::future<void> WorldSource::setPlayerInfo(const std::string &key, const std::vector<char> &value) {
    return this->work([&, key, value] {
        auto promise = this->reader->setPlayerInfo(this->playerId, key, value);
        auto future = promise.get_future();
        future.get();
    });
}

/**
 * Returns the player info value for the given key.
 */
std::promise<std::vector<char>> WorldSource::getPlayerInfo(const std::string &key) {
    return this->reader->getPlayerInfo(this->playerId, key);
}



/**
 * Returns world info value for the given key.
 */
std::promise<std::vector<char>> WorldSource::getWorldInfo(const std::string &key) {
    return this->reader->getWorldInfo(key);
}



/**
 * Determines chunks to write out.
 */
void WorldSource::startOfFrame() {
    if(this->inhibitDirtyChunkHandling) return;

    std::vector<std::pair<glm::ivec2, size_t>> toWrite;

    LOCK_GUARD(this->dirtyChunksLock, PickWriteChunks);

    // find all chunks that would hit the max frames since last dirtying
    for(auto &[pos, info] : this->dirtyChunks) {
        // did the timer expire?
        if(++info.framesSinceDirty == kDirtyThreshold) {
            toWrite.emplace_back(pos, info.totalFramesWaiting);
        }
        // is the age over the maximum?
        else if(++info.totalFramesWaiting > kMaxWriteRequestAge) {
            toWrite.emplace_back(pos, info.totalFramesWaiting);
        }
    }

    // sort by age and pick the oldest two
    std::sort(std::begin(toWrite), std::end(toWrite), [](const auto &l, const auto &r) {
        return (l.second > r.second);
    });

    if(toWrite.size() > kMaxWriteChunksPerFrame) {
        toWrite.resize(kMaxWriteChunksPerFrame);
    }

    // and go ahead and submit write requests for each
    for(const auto &[pos, age] : toWrite) {
        const auto &info = this->dirtyChunks[pos];

        WriteRequest req(info.chunk);
        this->writeQueue.enqueue(req);

        this->dirtyChunks.erase(pos);
    }
}

/**
 * Main loop for the modified chunks writing list.
 *
 * The chunk writer essentially listens on a queue of chunks to write out to the file or network;
 * which chunks are written is decided by the main loop.
 */
void WorldSource::writerMain() {
    using namespace std::chrono;

#if PROFILE
    MUtils::Profiler::NameThread("WorldSource Writer");
#endif
    util::Thread::setName("WorldSource Writer");

    while(this->workerRun) {
        // get a new write request
        WriteRequest req;
        this->writeQueue.wait_dequeue(req);

        if(!req.chunk) continue;
        if(this->generateOnly) continue;

        // write it
        const auto start = high_resolution_clock::now();

        auto prom = this->reader->putChunk(req.chunk);
        auto future = prom.get_future();
        future.get();

        const auto diff = high_resolution_clock::now() - start;
        const auto diffUs = duration_cast<microseconds>(diff).count();
        Logging::trace("Writing chunk {} took {} µS", req.chunk->worldPos, diffUs);

        // run completion handler if provided
        if(req.completion) {
            (*req.completion)();
        }
    }

#if PROFILE
    MUtils::Profiler::FinishThread();
#endif
}

/**
 * Marks a chunk as dirty.
 */
void WorldSource::markChunkDirty(std::shared_ptr<Chunk> &chunk) {
    XASSERT(chunk, "null chunk passed to WorldSource::markChunkDirty()!");

    LOCK_GUARD(this->dirtyChunksLock, DirtyChunks);
    PROFILE_SCOPE(MarkChunksDirty);

    // update an existing entry
    if(this->dirtyChunks.contains(chunk->worldPos)) {
        auto &entry = this->dirtyChunks[chunk->worldPos];
        entry.framesSinceDirty = 0;
        entry.numDirtyCounterResets++;
    }
    // insert a new entry
    else {
        this->dirtyChunks[chunk->worldPos] = {
            .chunk = chunk
        };
    }
}

/**
 * Checks whether the chunk is dirty before writing it synchronously.
 */
void WorldSource::forceChunkWriteIfDirtySync(std::shared_ptr<Chunk> &chunk) {
    XASSERT(chunk, "null chunk passed to WorldSource::forceChunkWriteIfDirtySync()!");
    PROFILE_SCOPE(WriteChunkIfDirtySync);

    // check if in dirty list
    {
        LOCK_GUARD(this->dirtyChunksLock, DirtyChunks);
        if(!this->dirtyChunks.contains(chunk->worldPos)) {
            return;
        }
    }

    this->forceChunkWriteSync(chunk);
}

/**
 * Forces the given chunk to be written out synchronously.
 */
void WorldSource::forceChunkWriteSync(std::shared_ptr<Chunk> &chunk) {
    XASSERT(chunk, "null chunk passed to WorldSource::forceChunkWriteSync()!");
    PROFILE_SCOPE(WriteChunkSync);

    // remove from dirty list
    {
        LOCK_GUARD(this->dirtyChunksLock, DirtyChunks);
        this->dirtyChunks.erase(chunk->worldPos);
    }

    // push write request
    std::promise<void> prom;

    WriteRequest req(chunk);
    req.completion = [&]() mutable {
        prom.set_value();
    };

    this->writeQueue.enqueue(req);

    // wait for write request to complete
    auto future = prom.get_future();
    future.get();
}

