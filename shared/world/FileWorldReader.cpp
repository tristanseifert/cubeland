#include "FileWorldReader.h"

#include "util/LZ4.h"
#include "util/Thread.h"
#include "io/Format.h"
#include <version.h>
#include <Logging.h>
#include <cmrc/cmrc.hpp>

#include <sqlite3.h>

#include <stdexcept>
#include <filesystem>
#include <time.h>

#if PROFILE
#include <mutils/time/profiler.h>
#else
#define PROFILE_SCOPE(x) 
#endif

CMRC_DECLARE(sql);

using namespace world;

/**
 * Attempts to read a world file from the given path. It is optionally created, if requested.
 *
 * We open the database file in the "no mutex" mode, to disable all internal synchronization on the
 * SQLite API calls. This is fine since we're serializing all accesses to our worker thread anyhow;
 * however, this scheme _does_ support opening further connections and implementing concurrency
 * that way.
 *
 * @note If creation is not requested, and the file doesn't exist, the request will fail.
 */
FileWorldReader::FileWorldReader(const std::string &path, const bool create, const bool readonly) : worldPath(path) {
    int err;

    // get the filename
    std::filesystem::path p(path);
    this->filename = p.filename().string();

    // open database connection
    int flags = (readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE) | SQLITE_OPEN_NOMUTEX;

    if(create) {
        flags |= SQLITE_OPEN_CREATE;
    }

    err = sqlite3_open_v2(path.c_str(), &this->db, flags, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error(f("Failed to open world: SQLite error {} ({})", 
                                   sqlite3_errstr(err), err));
    }

    // enable some pragmas
    err = sqlite3_exec(this->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error(f("Failed to enable foreign keys {} ({})", 
                                   sqlite3_errstr(err), err));
    }

    // allocate some additional stuff
    this->compressor = std::make_unique<util::LZ4>();

    // perform some mandatory initialization
    this->initializeSchema();
    this->loadBlockTypeMap();
    this->loadPlayerIds();

    // set up the worker thread
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&FileWorldReader::workerMain, this);

    this->acceptRequests = true;
}

/**
 * Clears up the file world reader.
 *
 * This notifies the background thread to shut down, sends it some dummy work, and waits for it to
 * terminate before returning.
 */
FileWorldReader::~FileWorldReader() {
    // shut down the worker
    this->acceptRequests = false;
    this->workerRun = false;

    this->sendWorkerNop();

    this->worker->join();
}




/**
 * Checks the database for the presense of the expected schema. If missing, we initialize it.
 */
void FileWorldReader::initializeSchema() {
    int err;

    // bail if the schema (the v1 info table) exists
    if(this->tableExists("worldinfo_v1")) {
        std::string creator = "?", version = "?", timestamp = "?";
        this->readWorldInfo("creator.name", creator);
        this->readWorldInfo("creator.version", version);
        this->readWorldInfo("creator.timestamp", timestamp);

        Logging::debug("World created by '{}' ({}) on {}", creator, version, timestamp);

        return;
    }

    // otherwise, just execute the big stored sql string
    Logging::trace("Initializing with v1 schema");

    auto fs = cmrc::sql::get_filesystem();
    auto file = fs.open("/world_v1.sql");
    std::string schema(file.begin(), file.end());

    err = sqlite3_exec(this->db, schema.c_str(), nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
        throw DbError(f("Failed to write schema ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    // set creator info
    this->updateWorldInfo("creator.name", "me.tseifert.cubeland");
    this->updateWorldInfo("creator.version", gVERSION_TAG);

    time_t now = time(nullptr);
    this->updateWorldInfo("creator.timestamp", f("{:d}", now));

    // generate a random world id
    std::random_device rand;
    auto seedData = std::array<int, std::mt19937::state_size> {};
    std::generate(std::begin(seedData), std::end(seedData), std::ref(rand));

    std::seed_seq seq(std::begin(seedData), std::end(seedData));

    std::mt19937 generator(seq);
    uuids::uuid_random_generator gen{generator};
    const uuids::uuid newId = gen();

    this->updateWorldInfo("world.id", uuids::to_string(newId));
}



/**
 * Worker thread main loop
 *
 * Pull work requests from the work queue until we're signalled to quit.
 */
void FileWorldReader::workerMain() {
    int err;

    const auto threadName = f("World: {}", this->filename);
#if PROFILE
    MUtils::Profiler::NameThread(threadName.c_str());
#endif
    util::Thread::setName(threadName);

    // wait for work to come in
    WorkItem item;
    while(this->workerRun) {
        this->workQueue.wait_dequeue(item);
        item.f();
    }

    // clean up
    PROFILE_SCOPE(Cleanup);
    if(this->db) {
        err = sqlite3_close(this->db);
        if(err != SQLITE_OK) {
            Logging::error("Failed to close world file: {} ({})", sqlite3_errstr(err), err);
        }
    }

    this->acceptRequests = false;
#if PROFILE
    MUtils::Profiler::FinishThread();
#endif
}

/**
 * Sends a no-op to the worker thread to wake it up.
 */
void FileWorldReader::sendWorkerNop() {
    this->workQueue.enqueue({
        .f = [&]{ /* nothing */ }
    });
}

/**
 * Determines the size of the database.
 */
std::promise<size_t> FileWorldReader::getDbSize() {
    this->canAcceptRequests();
    std::promise<size_t> prom;

    this->workQueue.enqueue({ .f = [&]{
        try {
            prom.set_value(this->getDbBytesUsed());
        } catch (std::exception &e) {
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}

/**
 * Determines whether we have a chunk at the given location.
 */
std::promise<bool> FileWorldReader::chunkExists(int x, int z) {
    this->canAcceptRequests();
    std::promise<bool> prom;

    this->workQueue.enqueue({ .f = [&, x, z]{
        try {
            prom.set_value(this->haveChunkAt(x, z));
        } catch (std::exception &e) {
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}

/**
 * Gets the min/max of the X/Z coords.
 */
std::promise<glm::vec4> FileWorldReader::getWorldExtents() {
    this->canAcceptRequests();
    std::promise<glm::vec4> prom;

    this->workQueue.enqueue({ .f = [&]{
        try {
            prom.set_value(this->getChunkBounds());
        } catch (std::exception &e) {
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}

/**
 * Reads a chunk from the world file.
 *
 * This reads the chunk, its metadata, and all slices that make up the blocks of the chunk. It's
 * then read into the in-memory representation used by the rest of the game engine.
 */
std::promise<std::shared_ptr<Chunk>> FileWorldReader::getChunk(int x, int z) {
    this->canAcceptRequests();
    std::promise<std::shared_ptr<Chunk>> prom;

    this->workQueue.enqueue({ .f = [&, x, z]{
        try {
            prom.set_value(this->loadChunk(x, z));
        } catch (std::exception &e) {
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}
/**
 * Writes a chunk to the world file.
 *
 * Existing chunks have their metadata and slice data replaced (deleting any slices that became
 * totally empty) if needed.
 */
std::promise<bool> FileWorldReader::putChunk(std::shared_ptr<Chunk> chunk) {
    this->canAcceptRequests();
    std::promise<bool> prom;

    this->workQueue.enqueue({ .f = [&, chunk]{
        // wrap the write in a transaction here. much cleaner
        try {
            this->beginTransaction();
            this->writeChunk(chunk);
            this->commitTransaction();

            prom.set_value(true);
        } catch (std::exception &e) {
            this->rollbackTransaction();
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}



/**
 * Checks whether a table with the given table exists in the database.
 */
bool FileWorldReader::tableExists(const std::string &name) {
    PROFILE_SCOPE(TableExists);

    int err;
    bool found = false;
    sqlite3_stmt *stmt = nullptr;

    // prepare query and bind the table name
    this->prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=?", &stmt);
    this->bindColumn(stmt, 1, name);

    // execute it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DbError(f("tableExists() failed to exec ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    found = (err == SQLITE_ROW);

    // clean up
    sqlite3_finalize(stmt);
    return found;
}

/**
 * Reads a world info value with the given key, and reads its value as a string.
 *
 * If there are multiple results, the first one is used.
 *
 * @return Whether the value was found or not.
 */
bool FileWorldReader::readWorldInfo(const std::string &key, std::string &value) {
    PROFILE_SCOPE(GetWorldInfo);

    int err;
    bool found = false;
    sqlite3_stmt *stmt = nullptr;

    std::vector<char> blobData;

    // prepare query and bind the key
    this->prepare("SELECT id,value FROM worldinfo_v1 WHERE name=?", &stmt);
    this->bindColumn(stmt, 1, key);

    // execute it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to step ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    if(err != SQLITE_ROW) {
        // no result = key not found
        goto beach;
    }

    // extract the blob data
    this->getColumn(stmt, 1, blobData);
    value = std::string(blobData.begin(), blobData.end());
    found = true;

beach:;
    // clean up
    sqlite3_finalize(stmt);
    return found;
}

/**
 * Sets a world info value to the given string value.
 */
void FileWorldReader::updateWorldInfo(const std::string &key, const std::string &value) {
    PROFILE_SCOPE(SetWorldInfo);

    int err;
    sqlite3_stmt *stmt = nullptr;

    // prepare query and bind the key/value
    this->prepare("INSERT INTO worldinfo_v1 (name, value, modified) VALUES (?, ?, CURRENT_TIMESTAMP) ON CONFLICT(name) DO UPDATE SET value=excluded.value, modified=CURRENT_TIMESTAMP;", &stmt);
    this->bindColumn(stmt, 1, key);
    this->bindColumn(stmt, 2, value);

    // execute it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to step ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    // clean up
    sqlite3_finalize(stmt);
}

/**
 * Queries SQLite for the size of pages, as well as the number of used pages.
 */
size_t FileWorldReader::getDbBytesUsed() {
    PROFILE_SCOPE(GetDbBytesUsed);

    int64_t bytes;
    int err;
    sqlite3_stmt *stmt = nullptr;

    this->prepare("SELECT page_count * page_size as size FROM pragma_page_count(), pragma_page_size();", &stmt);
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DbError(f("failed to step ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    if(err != SQLITE_ROW || !this->getColumn(stmt, 0, bytes)) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to get db usage");
    }

    sqlite3_finalize(stmt);
    return bytes;
}



/**
 * Checks whether we have a chunk at the given coordinate.
 */
bool FileWorldReader::haveChunkAt(int x, int z) {
    PROFILE_SCOPE(HaveChunkAt);

    int err;
    bool found = false;
    sqlite3_stmt *stmt = nullptr;
    int count;

    // prepare query and bind the key/value
    this->prepare("SELECT COUNT(id) FROM chunk_v1 WHERE worldX = ? AND worldZ = ?;", &stmt);
    this->bindColumn(stmt, 1, x);
    this->bindColumn(stmt, 2, z);

    err = sqlite3_step(stmt);

    if(err != SQLITE_ROW || !this->getColumn(stmt, 0, count)) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to get count");
    }

    found = (count > 0);

    // clean up
    sqlite3_finalize(stmt);
    return found;
}
/**
 * Gets the extents of the chunks in the world. In other words, it finds the smallest and largest
 * X and Z values at which there exist chunks.
 */
glm::vec4 FileWorldReader::getChunkBounds() {
    PROFILE_SCOPE(GetChunkBounds);

    int err;
    sqlite3_stmt *stmt = nullptr;
    glm::vec4 out(0);

    // prepare the query and send it
    this->prepare("SELECT MIN(worldX), MAX(worldX), MIN(worldZ), MAX(worldZ) from chunk_v1;", &stmt);

    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to get bounds: {}", err));
    }

    // get the values into the vector
    if(!this->getColumn(stmt, 0, out.x) || !this->getColumn(stmt, 1, out.y)) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to get X bounds");
    }

    if(!this->getColumn(stmt, 2, out.z) || !this->getColumn(stmt, 3, out.w)) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to get Z bounds");
    }

    // done
    sqlite3_finalize(stmt);
    return out;
}

/**
 * Loads the block type map.
 *
 * The block type map serves as a sort of compression, to take the 16-byte UUIDs that represent
 * blocks in the chunk, and convert them down to smaller 16-bit integers. This map is shared for
 * all chunks in the world.
 */
void FileWorldReader::loadBlockTypeMap() {
    PROFILE_SCOPE(LoadTypeMap);

    int err;
    sqlite3_stmt *stmt = nullptr;
    std::unordered_map<uint16_t, uuids::uuid> map;

    // prepare a query
    this->prepare("SELECT blockId, blockUuid FROM type_map_v1 ORDER BY blockId ASC;", &stmt);

    // sequentially read each row
    while((err = sqlite3_step(stmt)) == SQLITE_ROW) {
        int32_t key;
        uuids::uuid value;

        if(!this->getColumn(stmt, 0, key) || !this->getColumn(stmt, 1, value)) {
            throw std::runtime_error("Failed to get type map entry");
        }
        if(key > std::numeric_limits<uint16_t>::max()) {
            throw std::runtime_error(f("Invalid type map entry {} -> {}", key,
                        uuids::to_string(value)));
        }
        map[key] = value;

        if(key >= this->blockIdMapNext) {
            this->blockIdMapNext = (key + 1);
        }
    }

    // done; we've read all rows
    sqlite3_finalize(stmt);
    this->blockIdMap = map;
    this->blockIdMapDirty = false;
}
/**
 * Writes the block type map back out to the world file.
 *
 * As it's currently implemented, this will NOT remove existing block IDs, even if they are no
 * longer present in the type map. Only new types can be appended.
 */
void FileWorldReader::writeBlockTypeMap() {
    // bail if block map not dirty
    if(!this->blockIdMapDirty) return;

    PROFILE_SCOPE(WriteTypeMap);
    int err;
    sqlite3_stmt *stmt = nullptr;

    // prepare the queery
    this->prepare("INSERT INTO type_map_v1 (blockId, blockUuid, created) VALUES (?, ?, CURRENT_TIMESTAMP) ON CONFLICT(blockId) DO UPDATE SET blockUuid=excluded.blockUuid;", &stmt);

    // run a query for each entry in the map
    for(const auto &[blockId, blockUuid] : this->blockIdMap) {
        Logging::trace("Writing block id {} -> {}", blockId, uuids::to_string(blockUuid));

        this->bindColumn(stmt, 1, blockId);
        this->bindColumn(stmt, 2, blockUuid);

        err = sqlite3_step(stmt);
        if(err != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error(f("Failed to write block id map: {}", err));
        }

        // reset query
        err = sqlite3_reset(stmt);
        if(err != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error(f("Failed to reset block id map query: {}", err));
        }
    }

    // clean up
    sqlite3_finalize(stmt);
    this->blockIdMapDirty = false;
}



/**
 * Reads a player info key.
 */
std::promise<std::vector<char>> FileWorldReader::getPlayerInfo(const uuids::uuid &player,
        const std::string &key) {
    this->canAcceptRequests();
    std::promise<std::vector<char>> prom;

    this->workQueue.enqueue({ .f = [&, player, key]{
        try {
            // TODO: figure out a better way to return "not found" than a 0 byte result
            std::vector<char> data;
            bool found = this->readPlayerInfo(player, key, data);
            if(!found) {
                data.clear();
            }
            prom.set_value(data);
        } catch (std::exception &e) {
            Logging::error("Failed to read player info: {}", e.what());
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}

/**
 * Writes player info. Note that we must have a mapping from player id to row id.
 */
std::promise<void> FileWorldReader::setPlayerInfo(const uuids::uuid &player, const std::string &key, const std::vector<char> &data) {
    this->canAcceptRequests();
    std::promise<void> prom;

    this->workQueue.enqueue({ .f = [&, player, key, data]{
        try {
            this->updatePlayerInfo(player, key, data);
            prom.set_value();
        } catch (std::exception &e) {
            Logging::error("Failed to write player info: {}", e.what());
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}



/**
 * Gets a world info key.
 */
std::promise<std::vector<char>> FileWorldReader::getWorldInfo(const std::string &key) {
    this->canAcceptRequests();
    std::promise<std::vector<char>> prom;

    this->workQueue.enqueue({ .f = [&, key]{
        try {
            std::vector<char> data;
            std::string temp = "?";

            if(this->readWorldInfo(key, temp)) {
                data.assign(temp.begin(), temp.end());
            } else {
                data.clear();
            }

            prom.set_value(data);
        } catch (std::exception &e) {
            Logging::error("Failed to read world info: {}", e.what());
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}

/**
 * Writes out a world info key.
 */
std::promise<void> FileWorldReader::setWorldInfo(const std::string &key, const std::vector<char> &data) {
    this->canAcceptRequests();
    std::promise<void> prom;

    this->workQueue.enqueue({ .f = [&, key, data]{
        try {
            const std::string str(data.begin(), data.end());
            this->updateWorldInfo(key, str);
            prom.set_value();
        } catch (std::exception &e) {
            Logging::error("Failed to write world info: {}", e.what());
            prom.set_exception(std::current_exception());
        }
    }});

    return prom;
}


