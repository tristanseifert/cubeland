#include "ChunkWorker.h"

#include "util/ThreadPool.h"

#include "io/PrefsManager.h"
#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>

using namespace render::chunk;

/// shared instance
std::shared_ptr<ChunkWorker> ChunkWorker::gShared;



/**
 * Create the thread pool on initialization
 */
ChunkWorker::ChunkWorker() : ThreadPool("Chunk Worker") {
    // read number of workers from preferences
    unsigned int hwThreads = std::thread::hardware_concurrency() / 2;
    unsigned int fallback = std::min(hwThreads, 5U);

    this->numWorkers = io::PrefsManager::getUnsigned("chunk.drawWorkThreads", fallback);

    // create workers
    this->startWorkers(this->numWorkers);
}

/**
 * When deallocating, make sure the thread pool is shut down cleanly
 */
ChunkWorker::~ChunkWorker() {
    for(size_t i = 0; i < this->numWorkers+1; i++) {
        this->pushNop();
    }
}

/**
 * Sets the thread's names.
 */
void ChunkWorker::workerThreadStarted(const size_t i) {
    const auto threadName = f("ChunkWorker {}", i+1);
    MUtils::Profiler::NameThread(threadName.c_str());
}

/**
 * Cleans up the thread's profiler data.
 */
void ChunkWorker::workerThreadWillEnd(const size_t) {
    MUtils::Profiler::FinishThread();
}
