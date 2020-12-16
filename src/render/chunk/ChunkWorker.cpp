#include "ChunkWorker.h"

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
    // calculate number of workers: half the CPU cores up to a max of 5
    unsigned int hwThreads = std::thread::hardware_concurrency() / 2;
    this->numWorkers = std::min(hwThreads, 5U);

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

    for(size_t i = 0; i < this->workers.size()+1; i++) {
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
