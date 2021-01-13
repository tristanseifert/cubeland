#ifndef WORLD_FILEWORLDREADER_H
#define WORLD_FILEWORLDREADER_H

#include "WorldReader.h"
#include "util/SQLite.h"

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>
#include <stdexcept>
#include <array>
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
struct Chunk;
struct ChunkSlice;
struct ChunkSliceFileBlockMeta;

/**
 * Supports reading world data from a file on disk. This file is in essence an sqlite3 database.
 *
 * All BLOB fields are compressed with LZ4 framed format, unless otherwise specified. Complex data
 * is archived using the portable binary archivers from the cereal library.
 */
class FileWorldReader: public WorldReader {
    friend class WorldDebugger;

    public:
        FileWorldReader() = delete;
        FileWorldReader(const std::string &path, const bool create = false);

        ~FileWorldReader();

    public:
        std::promise<size_t> getDbSize();

        std::promise<bool> chunkExists(int x, int z) override;
        std::promise<glm::vec4> getWorldExtents() override;
        std::promise<std::shared_ptr<Chunk>> getChunk(int x, int z) override;
        std::promise<bool> putChunk(std::shared_ptr<Chunk> chunk) override;

        std::promise<std::vector<char>> getPlayerInfo(const uuids::uuid &player, const std::string &key) override;
        std::promise<void> setPlayerInfo(const uuids::uuid &player, const std::string &key, const std::vector<char> &data) override;

        std::promise<std::vector<char>> getWorldInfo(const std::string &key) override;
        std::promise<void> setWorldInfo(const std::string &key, const std::vector<char> &data) override;
        std::promise<void> setWorldInfo(const std::string &key, const std::string &data) override {
            return WorldReader::setWorldInfo(key, data);
        }

    // these are the DB-context relative functions of the above
    private:
        bool haveChunkAt(int, int);
        glm::vec4 getChunkBounds();

    // shared chunk IO functions
    private:
        void getSlicesForChunk(const int chunkId, std::unordered_map<int, int> &slices);

    // chunk writing functions
    private:
        void writeChunk(const std::shared_ptr<Chunk> &);
        void serializeChunkMeta(const std::shared_ptr<Chunk> &chunk, std::vector<char> &data);

        void removeSlice(const int sliceId);
        void insertSlice(const std::shared_ptr<Chunk> &chunk, const int chunkId, const ChunkSliceFileBlockMeta &, const int y);
        void updateSlice(const int sliceId, const std::shared_ptr<Chunk> &chunk, const ChunkSliceFileBlockMeta &, const int y);

        void serializeSliceBlocks(const std::shared_ptr<Chunk> &chunk, const int y, std::vector<char> &data);
        void buildFileIdMap(std::unordered_map<uuids::uuid, uint16_t> &);
        void serializeSliceMeta(const std::shared_ptr<Chunk> &chunk, const int y, const ChunkSliceFileBlockMeta &, std::vector<char> &data);

        void extractBlockMeta(const std::shared_ptr<Chunk> &chunk, std::array<ChunkSliceFileBlockMeta, 256> &meta);

    // chunk reading functions
    private:
        struct SliceState {
            // generated 8 -> 16 maps
            std::vector<std::array<uint16_t, 256>> maps;
            // same as above, but instead map 16 -> 8
            std::vector<std::unordered_map<uint16_t, uint8_t>> reverseMaps;
        };

        std::shared_ptr<Chunk> loadChunk(int, int);

        void deserializeChunkMeta(std::shared_ptr<Chunk> chunk, const std::vector<char> &bytes);

        void loadSlice(SliceState &state, const int sliceId, std::shared_ptr<Chunk> chunk, const int y);
        void deserializeSliceBlocks(std::shared_ptr<Chunk> chunk, const int y, const std::vector<char> &data);
        void deserializeSliceMeta(std::shared_ptr<Chunk> chunk, const int y, const std::vector<char> &data);

        void processSliceRow(SliceState &state, std::shared_ptr<Chunk> chunk, ChunkSlice *slice, const size_t z);

    // misc metadata functions
    private:
        void loadBlockTypeMap();
        void writeBlockTypeMap();

        void loadPlayerIds();
        void insertPlayerId(const uuids::uuid &player);
        void updatePlayerInfo(const uuids::uuid &player, const std::string &key, const std::vector<char> &data);
        bool readPlayerInfo(const uuids::uuid &player, const std::string &key, std::vector<char> &data);

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

        bool readWorldInfo(const std::string &key, std::string &value);
        void updateWorldInfo(const std::string &key, const std::string &value);

        size_t getDbBytesUsed();

        void prepare(const std::string &query, sqlite3_stmt **out) {
            util::SQLite::prepare(this->db, query, out);
        }

        /// Sets a query parameter
        template <class T> void bindColumn(sqlite3_stmt *stmt, const size_t idx, T const &value) {
            util::SQLite::bindColumn(stmt, idx, value);
        }
        /// Gets a column value
        template <class T> bool getColumn(sqlite3_stmt *stmt, const size_t idx, T &out) {
            return util::SQLite::getColumn(stmt, idx, out);
        }

        void beginTransaction() {
            util::SQLite::beginTransaction(this->db);
        }
        void rollbackTransaction() {
            util::SQLite::rollbackTransaction(this->db);
        }
        void commitTransaction() {
            util::SQLite::commitTransaction(this->db);
        }

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
        /// when set, we need to write out the block id map
        bool blockIdMapDirty = false;
        /// next free block id value
        uint16_t blockIdMapNext = 1;

        /// world filename
        std::string filename;
        /// path from which the world file is loaded
        std::string worldPath;

        /// work buffer used for serializing block layout. may only be accessed from worker
        std::array<uint16_t, (256*256)> sliceTempGrid;
        /// decompression scratch buffer
        std::vector<char> scratch;

        // cache of all player uuid -> player object IDs
        std::unordered_map<uuids::uuid, int64_t> playerIds;

        /// used for decompressing/compressing block data
        std::unique_ptr<util::LZ4> compressor;
};
}

#endif
