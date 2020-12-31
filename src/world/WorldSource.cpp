#include "WorldSource.h"
#include "WorldReader.h"
#include "WorldGenerator.h"

#include "chunk/Chunk.h"
#include "chunk/ChunkSlice.h"
#include "io/Format.h"
#include "io/PrefsManager.h"

#include <Logging.h>
#include <mutils/time/profiler.h>

#include <utility>
#include <chrono>

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
        const size_t _numThreads) : generator(_g), reader(_r) {
    // set up threads
    size_t numThreads = _numThreads;
    if(!numThreads) {
        numThreads = io::PrefsManager::getUnsigned("world.sourceWorkThreads", 2);
    }

    this->numWorkers = numThreads;
    this->workerRun = true;

    for(size_t i = 0; i < this->numWorkers; i++) {
        auto worker = std::make_unique<std::thread>(&WorldSource::workerMain, this, i);
        this->workers.push_back(std::move(worker));
    }

    // set up default player id
    this->playerId = uuids::uuid::from_string("B8B0B551-8BF5-4F06-9C56-3A540120E8E5");

    // set up some additional initial state
    this->generateOnly = false;
    this->acceptRequests = true;

    // set up the writer thread
    this->writerThread = std::make_unique<std::thread>(&WorldSource::writerMain, this);
}

/**
 * Ensures all work threads are shut down cleanly.
 */
WorldSource::~WorldSource() {
    // force all chunks to finish writing
    if(!this->dirtyChunks.empty()) {
        Logging::info("Waiting for {} dirty chunk(s) to finish writing", this->dirtyChunks.size());
        for(auto &[pos, info] : this->dirtyChunks) {
            this->forceChunkWriteSync(info.chunk);
        }
    }

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
    MUtils::Profiler::NameThread(threadName.c_str());

    // main loop; dequeue work items
    WorkItem item;
    while(this->workerRun) {
        this->workQueue.wait_dequeue(item);
        item();
    }
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
 * Determines chunks to write out.
 */
void WorldSource::startOfFrame() {
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

    while(this->workerRun) {
        // get a new write request
        WriteRequest req;
        this->writeQueue.wait_dequeue(req);

        if(!req.chunk) continue;
        if(this->generateOnly) continue;

        // write it
        const auto start = high_resolution_clock::now();

        auto prom = this->reader->putChunk(req.chunk);
        prom.get_future().get();

        const auto diff = high_resolution_clock::now() - start;
        const auto diffUs = duration_cast<microseconds>(diff).count();
        Logging::trace("Writing chunk {} took {} µS", req.chunk->worldPos, diffUs);

        // run completion handler if provided
        if(req.completion) {
            (*req.completion)();
        }
    }
}

/**
 * Marks a chunk as dirty.
 */
void WorldSource::markChunkDirty(std::shared_ptr<Chunk> &chunk) {
    XASSERT(chunk, "null chunk passed to WorldSource::markChunkDirty()!");
    LOCK_GUARD(this->dirtyChunksLock, MarkChunkDirty);

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
 * Forces the given chunk to be written out synchronously.
 */
void WorldSource::forceChunkWriteSync(std::shared_ptr<Chunk> &chunk) {
    XASSERT(chunk, "null chunk passed to WorldSource::forceChunkWriteSync()!");

    // remove from dirty list
    {
        LOCK_GUARD(this->dirtyChunksLock, RemoveForceWriteChunk);
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
    prom.get_future().get();
}

