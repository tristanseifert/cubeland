#ifndef WORLD_FILEWORLDREADER_H
#define WORLD_FILEWORLDREADER_H

#include "WorldReader.h"

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>
#include <stdexcept>
#include <future>
#include <unordered_map>
#include <cstdint>

#include <blockingconcurrentqueue.h>
#include <glm/vec4.hpp>
#include <uuid.h>

struct sqlite3;
struct sqlite3_stmt;

namespace util {
class LZ4;
}

namespace world {
class WorldDebugger;

/**
 * Supports reading world data from a file on disk. This file is in essence an sqlite3 database.
 */
class FileWorldReader: public WorldReader {
    friend class WorldDebugger;

    public:
        FileWorldReader() = delete;
        FileWorldReader(const std::string &path, const bool create = false);

        ~FileWorldReader();

    public:
        std::promise<size_t> getDbSize();

        std::promise<bool> chunkExists(int x, int z);
        std::promise<glm::vec4> getWorldExtents();
        std::promise<std::shared_ptr<Chunk>> getChunk(int x, int z);
        std::promise<bool> putChunk(std::shared_ptr<Chunk> chunk);

    // these are the DB-context relative functions of the above
    private:
        bool haveChunkAt(int, int);
        glm::vec4 getChunkBounds();

        std::shared_ptr<Chunk> loadChunk(int, int);

        void writeChunk(std::shared_ptr<Chunk>);
        void getSlicesForChunk(const int chunkId, std::unordered_map<int, int> &slices);

        void removeSlice(const int sliceId);
        void insertSlice(std::shared_ptr<Chunk> chunk, const int y);
        void updateSlice(const int sliceId, std::shared_ptr<Chunk> chunk, const int y);

        void loadBlockTypeMap();
        void writeBlockTypeMap();

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
    
    private:
        // ensure we can accept requests at the moment
        void canAcceptRequests() {
            if(!this->acceptRequests) {
                throw std::runtime_error("Not accepting requests");
            }
        }

    // methods in this block must be called from the current db thread
    private:
        void initializeSchema();

        bool tableExists(const std::string &name);

        bool getWorldInfo(const std::string &key, std::string &value);
        void setWorldInfo(const std::string &key, const std::string &value);

        size_t getDbBytesUsed();

        void prepare(const std::string &, sqlite3_stmt **);

        void bindColumn(sqlite3_stmt *, const size_t, const std::string &);
        void bindColumn(sqlite3_stmt *, const size_t, const std::vector<unsigned char> &);
        void bindColumn(sqlite3_stmt *, const size_t, const uuids::uuid &);
        void bindColumn(sqlite3_stmt *, const size_t, const double);
        void bindColumn(sqlite3_stmt *, const size_t, const int32_t);
        void bindColumn(sqlite3_stmt *, const size_t, const int64_t);
        void bindColumn(sqlite3_stmt *, const size_t, std::nullptr_t);
        void bindColumn(sqlite3_stmt *stmt, const size_t i, const bool value) {
            this->bindColumn(stmt, i, (int32_t) (value ? 1 : 0));
        }
        void bindColumn(sqlite3_stmt *stmt, const size_t i, const float value) {
            this->bindColumn(stmt, i, (double) value);
        }

        bool getColumn(sqlite3_stmt *, const size_t, std::string &);
        bool getColumn(sqlite3_stmt *, const size_t, std::vector<unsigned char> &);
        bool getColumn(sqlite3_stmt *, const size_t, uuids::uuid &);
        bool getColumn(sqlite3_stmt *, const size_t, double &);
        bool getColumn(sqlite3_stmt *, const size_t, int32_t &);
        bool getColumn(sqlite3_stmt *, const size_t, int64_t &);
        bool getColumn(sqlite3_stmt *, const size_t, bool &);

        void beginTransaction();
        void rollbackTransaction();
        void commitTransaction();

        /// Gets a double (REAL) column as a float.
        bool getColumn(sqlite3_stmt *stmt, const size_t col, float &value) {
            double temp;
            if(this->getColumn(stmt, col, temp)) {
                value = (float) temp;
                return true;
            }
            return false;
        }

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

        /// mapping of 16-bit block ID -> game block UUID
        std::unordered_map<uint16_t, uuids::uuid> blockIdMap;

        /// world filename
        std::string filename;
        /// path from which the world file is loaded
        std::string worldPath;

        /// used for decompressing/compressing block data
        std::unique_ptr<util::LZ4> compressor;
};
}

#endif
