#include "ChunkWorker.h"

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
ChunkWorker::ChunkWorker() {
    // read number of workers from preferences
    unsigned int hwThreads = std::thread::hardware_concurrency() / 2;
    unsigned int fallback = std::min(hwThreads, 5U);

    this->numWorkers = io::PrefsManager::getUnsigned("chunk.drawWorkThreads", fallback);
    Logging::debug("Using {} chunk workers", this->numWorkers);

    // start workers
    this->workerRun = true;

    for(size_t i = 0; i < this->numWorkers; i++) {
        auto worker = std::make_unique<std::thread>(&ChunkWorker::workerMain, this, i);
        this->workers.push_back(std::move(worker));
    }

    // once the workers are started, allow accepting requests
    this->acceptRequests = true;
}

/**
 * When deallocating, make sure the thread pool is shut down cleanly
 */
ChunkWorker::~ChunkWorker() {
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
 * Main loop for the worker thread
 */
void ChunkWorker::workerMain(size_t i) {
    // perform some setup
    const auto threadName = f("ChunkWorker {}", i+1);
    MUtils::Profiler::NameThread(threadName.c_str());

    // main loop; dequeue work items
    WorkItem item;
    while(this->workerRun) {
        this->workQueue.wait_dequeue(item);
        item();
    }
}
