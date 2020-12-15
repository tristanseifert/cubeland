#include "FileWorldReader.h"

#include "io/Format.h"
#include <version.h>
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <cmrc/cmrc.hpp>

#include <sqlite3.h>

#include <stdexcept>
#include <filesystem>
#include <time.h>

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
FileWorldReader::FileWorldReader(const std::string &path, const bool create) : worldPath(path) {
    int err;

    // get the filename
    std::filesystem::path p(path);
    this->filename = p.filename().string();

    // open database connection
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX;

    if(create) {
        flags |= SQLITE_OPEN_CREATE;
    }

    Logging::trace("Attempting to open world: {} (create: {})", path, create);
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

    // perform some mandatory initialization
    this->initializeSchema();

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
        Logging::trace("World has v1 schema");

        std::string creator = "?", version = "?", timestamp = "?";
        this->getWorldInfo("creator.name", creator);
        this->getWorldInfo("creator.version", version);
        this->getWorldInfo("creator.timestamp", timestamp);
        
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
    this->setWorldInfo("creator.name", "me.tseifert.cubeland");
    this->setWorldInfo("creator.version", gVERSION_HASH);

    time_t now = time(nullptr);
    this->setWorldInfo("creator.timestamp", f("{:d}", now));
}



/**
 * Worker thread main loop
 *
 * Pull work requests from the work queue until we're signalled to quit.
 */
void FileWorldReader::workerMain() {
    int err;

    const auto threadName = f("World: {}", this->filename);
    MUtils::Profiler::NameThread(threadName.c_str());

    // wait for work to come in
    WorkItem item;
    while(this->workerRun) {
        PROFILE_SCOPE(WorkLoop);

        this->workQueue.wait_dequeue(item);

        PROFILE_SCOPE(Callout);
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
    Logging::trace("File world reader worker exiting");

    this->acceptRequests = false;
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
    if(!this->acceptRequests) {
        throw std::runtime_error("Not accepting requests");
    }

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
    if(!this->acceptRequests) {
        throw std::runtime_error("Not accepting requests");
    }
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
    if(!this->acceptRequests) {
        throw std::runtime_error("Not accepting requests");
    }
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
bool FileWorldReader::getWorldInfo(const std::string &key, std::string &value) {
    PROFILE_SCOPE(GetWorldInfo);

    int err;
    bool found = false;
    sqlite3_stmt *stmt = nullptr;

    std::vector<unsigned char> blobData;

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

beach:;
    // clean up
    sqlite3_finalize(stmt);
    return found;
}

/**
 * Sets a world info value to the given string value.
 */
void FileWorldReader::setWorldInfo(const std::string &key, const std::string &value) {
    PROFILE_SCOPE(SetWorldInfo);

    int err;
    bool found = false;
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
beach:;
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
