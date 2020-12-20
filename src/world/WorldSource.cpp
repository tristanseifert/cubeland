#include "WorldSource.h"
#include "WorldReader.h"
#include "WorldGenerator.h"

#include "chunk/Chunk.h"
#include "chunk/ChunkSlice.h"
#include "io/Format.h"
#include "io/PrefsManager.h"

#include <mutils/time/profiler.h>

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

    // set up some additional initial state
    this->generateOnly = false;
    this->acceptRequests = true;
}

/**
 * Ensures all work threads are shut down cleanly.
 */
WorldSource::~WorldSource() {
    // set stop flag and queue nThreads+1 NOPs
    this->acceptRequests = false;
    this->workerRun = false;

    for(size_t i = 0; i < this->numWorkers+1; i++) {
        this->pushNop();
    }

    // wait for all threads to join
    for(auto &thread : this->workers) {
        thread->join();
    }
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
            return chunk.get_future().get();
        }
    }

    // if we get here, we need to invoke the generatour
    return this->generator->generateChunk(x, z);
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

