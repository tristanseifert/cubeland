#include "FileWorldReader.h"

#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <cmrc/cmrc.hpp>

#include <sqlite3.h>

#include <stdexcept>
#include <filesystem>

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

    // ready to accept requests
    this->acceptRequests = true;

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
 * Checks whether a table with the given table exists in the database.
 */
bool FileWorldReader::tableExists(const std::string &name) {
    PROFILE_SCOPE(TableExists);

    int err;
    bool found = false;
    sqlite3_stmt *stmt = nullptr;

    // prepare query and bind the table name
    static const char *kQuery = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    static const size_t kQueryLen = strlen(kQuery) + 1;

    err = sqlite3_prepare_v2(this->db, kQuery, -kQueryLen, &stmt, nullptr);
    if(err != SQLITE_OK) {
        throw DbError(f("tableExists() failed to prepare ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    err = sqlite3_bind_text(stmt, 1, name.c_str(), name.size(), nullptr);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw DbError(f("tableExists() failed to bind ({}): {}", err, sqlite3_errmsg(this->db)));
    }

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

