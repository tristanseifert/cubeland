#ifndef WORLD_FILEWORLDREADER_H
#define WORLD_FILEWORLDREADER_H

#include "WorldReader.h"

#include <blockingconcurrentqueue.h>

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>
#include <stdexcept>

struct sqlite3;

namespace world {
/**
 * Supports reading world data from a file on disk. This file is in essence an sqlite3 database.
 */
class FileWorldReader: public WorldReader {
    public:
        FileWorldReader() = delete;
        FileWorldReader(const std::string &path, const bool create = false);

        ~FileWorldReader();

    private:
        class DbError: public std::runtime_error {
            public:
                DbError(const std::string &what) : std::runtime_error(what) {};
        };

    private:
        /// Individual piece of work sent to the worker
        struct WorkItem {
            std::function<void(void)> f;
        };

    // methods in this block must be called from the current db thread
    private:
        void initializeSchema();

        bool tableExists(const std::string &name);

    private:
        void workerMain();
        void sendWorkerNop();

    private:
        /// worker thread processes requests as long as this is set
        std::atomic_bool workerRun;
        /// worker thread 
        std::unique_ptr<std::thread> worker = nullptr;
        /// work requests sent to the thread
        moodycamel::BlockingConcurrentQueue<WorkItem> workQueue;

        /// worker thread database connection
        sqlite3 *db = nullptr;

        /// accepts requests as long as this is set; checked at the start of each WorldReader call
        std::atomic_bool acceptRequests;

        /// world filename
        std::string filename;
        /// path from which the world file is loaded
        std::string worldPath;
};
}

#endif
