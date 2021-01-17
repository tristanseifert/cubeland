#include "PrefsManager.h"
#include "io/PathHelper.h"
#include "io/Format.h"

#include "util/SQLite.h"

#include <Logging.h>

#include <sqlite3.h>
#include <cmrc/cmrc.hpp>

#include <stdexcept>
#include <iostream>
#include <filesystem>

using namespace io;
using namespace util;

CMRC_DECLARE(sql);

std::unique_ptr<PrefsManager> PrefsManager::shared = nullptr;

/**
 * Sets the default preferences path.
 */
PrefsManager::PrefsManager() {
    int err;

    // open the prefs database
    this->path = PathHelper::appDataDir() + "/preferences.sqlite3";

    err = sqlite3_open_v2(this->path.c_str(), &this->db, 
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error(f("Failed to open preferences (from {}): {}", this->path,
                sqlite3_errstr(err)));
    }

    // set up schema
    this->initSchema();

    // apply the defaults for any missing prefs
    // this->loadDefaults();
}

/**
 * Closes the prefs database.
 */
PrefsManager::~PrefsManager() {
    int err;

    // close down the datablaze
    std::lock_guard<std::mutex> lg(this->lock);

    err = sqlite3_close(this->db);
    if(err != SQLITE_OK) {
        Logging::error("Failed to close preferences file: {}", err);
    }

    this->db = nullptr;
}

/**
 * Sets up the initial database schema.
 */
void PrefsManager::initSchema() {
    int err;

    // bail if the schema (the v1 info table) exists
    if(SQLite::tableExists(this->db, "prefs_string_v1")) {
        return;
    }

    // otherwise, just execute the big stored sql string
    auto fs = cmrc::sql::get_filesystem();
    auto file = fs.open("/prefs_v1.sql");
    std::string schema(file.begin(), file.end());

    SQLite::beginTransaction(this->db);

    err = sqlite3_exec(this->db, schema.c_str(), nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error(f("Failed to write preferences schema ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    SQLite::commitTransaction(this->db);
}

/**
 * Loads default settings.
 */
void PrefsManager::loadDefaults() {
    // create the "window" section
    /*if(!this->config->exists("window")) {
        this->config->getRoot().add("window", libconfig::Setting::TypeGroup);
    }

    {
        auto &s= this->config->lookup("window");
        if(!s.exists("restoreSize")) {
            s.add("restoreSize", libconfig::Setting::TypeBoolean) = false;
        }
        if(!s.exists("width")) {
            s.add("width", libconfig::Setting::TypeInt64) = 1024L;
        }
        if(!s.exists("height")) {
            s.add("height", libconfig::Setting::TypeInt64) = 768L;
        }
    }

    // world IO and generation section
    {
        if(!this->config->exists("world")) {
            this->config->getRoot().add("world", libconfig::Setting::TypeGroup);
        }
        auto &s = this->config->lookup("world");
        if(!s.exists("sourceWorkThreads")) {
            // number of worker threads for world sources (generation mostly)
            s.add("sourceWorkThreads", libconfig::Setting::TypeInt64) = 2L;
        }
    }

    // chunk processing section
    {
        if(!this->config->exists("chunk")) {
            this->config->getRoot().add("chunk", libconfig::Setting::TypeGroup);
        }
        auto &s = this->config->lookup("chunk");
        if(!s.exists("drawWorkThreads")) {
            // number of worker threads used by the WorldChunk work queue (rendering/physics)
            s.add("drawWorkThreads", libconfig::Setting::TypeInt64) = 3L;
        }
    }*/
}

/**
 * Writes the current set of preferences out to disk.
 */
void PrefsManager::write() {
    // there's really nothing to do here. shoutout sqlite
}

/**
 * Gets a uuid value.
 */
const std::optional<uuids::uuid> PrefsManager::getUuid(const std::string &key) {
    sqlite3_stmt *stmt = nullptr;

    // prepare the query
    std::lock_guard<std::mutex> guard(shared->lock);

    SQLite::prepare(shared->db, "SELECT value FROM prefs_uuid_v1 WHERE key = ?;", &stmt);
    SQLite::bindColumn(stmt, 1, key);

    int err = sqlite3_step(stmt);

    // no such row, return default
    if(err == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    } 
    // got a row with this key, get its value
    else if(err == SQLITE_ROW) {
        uuids::uuid temp;
        SQLite::getColumn(stmt, 0, temp);
        sqlite3_finalize(stmt);
        return temp;
    } 
    // DB error
    else {
        throw std::runtime_error(f("Failed to read preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }
}
/**
 * Sets the given blob value.
 */
void PrefsManager::setUuid(const std::string &key, const uuids::uuid &value) {
    sqlite3_stmt *stmt = nullptr;

    std::lock_guard<std::mutex> lg(shared->lock);

    SQLite::prepare(shared->db, "INSERT INTO prefs_uuid_v1 (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value, modified=CURRENT_TIMESTAMP;", &stmt);
    SQLite::bindColumn(stmt, 1, key);
    SQLite::bindColumn(stmt, 2, value);

    int err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to write preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }

    sqlite3_finalize(stmt);
}



/**
 * Gets a blob value.
 */
const std::optional<std::vector<unsigned char>> PrefsManager::getBlob(const std::string &key) {
    sqlite3_stmt *stmt = nullptr;

    // prepare the query
    std::lock_guard<std::mutex> guard(shared->lock);

    SQLite::prepare(shared->db, "SELECT value FROM prefs_blob_v1 WHERE key = ?;", &stmt);
    SQLite::bindColumn(stmt, 1, key);

    int err = sqlite3_step(stmt);

    // no such row, return default
    if(err == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    } 
    // got a row with this key, get its value
    else if(err == SQLITE_ROW) {
        std::vector<unsigned char> temp;
        SQLite::getColumn(stmt, 0, temp);
        sqlite3_finalize(stmt);
        return temp;
    } 
    // DB error
    else {
        throw std::runtime_error(f("Failed to read preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }
}
/**
 * Sets the given blob value.
 */
void PrefsManager::setBlob(const std::string &key, const std::vector<unsigned char> &value) {
    sqlite3_stmt *stmt = nullptr;
    std::lock_guard<std::mutex> lg(shared->lock);

    SQLite::prepare(shared->db, "INSERT INTO prefs_blob_v1 (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value, modified=CURRENT_TIMESTAMP;", &stmt);
    SQLite::bindColumn(stmt, 1, key);
    SQLite::bindColumn(stmt, 2, value);

    int err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to write preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }

    sqlite3_finalize(stmt);
}

/**
 * Deletes the given blob key.
 */
void PrefsManager::deleteBlob(const std::string &key) {
    sqlite3_stmt *stmt = nullptr;
    std::lock_guard<std::mutex> lg(shared->lock);

    SQLite::prepare(shared->db, "DELETE prefs_blob_v1 WHERE key = ?", &stmt);
    SQLite::bindColumn(stmt, 1, key);
 
    int err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to remove preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }

    sqlite3_finalize(stmt);

} 


/**
 * Gets a string value.
 */
const std::string PrefsManager::getString(const std::string &key, const std::string &fallback) {
    sqlite3_stmt *stmt = nullptr;

    // prepare the query
    std::lock_guard<std::mutex> guard(shared->lock);

    SQLite::prepare(shared->db, "SELECT value FROM prefs_text_v1 WHERE key = ?;", &stmt);
    SQLite::bindColumn(stmt, 1, key);

    int err = sqlite3_step(stmt);

    // no such row, return default
    if(err == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return fallback;
    } 
    // got a row with this key, get its value
    else if(err == SQLITE_ROW) {
        std::string temp;
        SQLite::getColumn(stmt, 0, temp);
        sqlite3_finalize(stmt);
        return temp;
    } 
    // DB error
    else {
        throw std::runtime_error(f("Failed to read preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }
}
/**
 * Sets the given string value.
 */
void PrefsManager::setString(const std::string &key, const std::string &value) {
    sqlite3_stmt *stmt = nullptr;

    std::lock_guard<std::mutex> lg(shared->lock);

    SQLite::prepare(shared->db, "INSERT INTO prefs_text_v1 (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value, modified=CURRENT_TIMESTAMP;", &stmt);
    SQLite::bindColumn(stmt, 1, key);
    SQLite::bindColumn(stmt, 2, value);

    int err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to write preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }

    sqlite3_finalize(stmt);
}



/**
 * Gets an unsigned integer value.
 */
const unsigned int PrefsManager::getUnsigned(const std::string &key, const unsigned int fallback) {
    int err;
    sqlite3_stmt *stmt = nullptr;

    // prepare the query
    std::lock_guard<std::mutex> guard(shared->lock);

    SQLite::prepare(shared->db, "SELECT value FROM prefs_number_v1 WHERE key = ?;", &stmt);
    SQLite::bindColumn(stmt, 1, key);

    err = sqlite3_step(stmt);

    // no such row, return default
    if(err == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return fallback;
    } 
    // got a row with this key, get its value
    else if(err == SQLITE_ROW) {
        int64_t temp = 0;

        SQLite::getColumn(stmt, 0, temp);

        sqlite3_finalize(stmt);
        return temp;
    } 
    // DB error
    else {
        throw std::runtime_error(f("Failed to read preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }
}
/**
 * Sets the given unsigned value.
 */
void PrefsManager::setUnsigned(const std::string &key, const unsigned int value) {
    int err;
    sqlite3_stmt *stmt = nullptr;

    std::lock_guard<std::mutex> lg(shared->lock);

    SQLite::prepare(shared->db, "INSERT INTO prefs_number_v1 (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value, modified=CURRENT_TIMESTAMP;", &stmt);
    SQLite::bindColumn(stmt, 1, key);
    SQLite::bindColumn(stmt, 2, (int64_t) value);

    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to write preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }

    // clean up
    sqlite3_finalize(stmt);
}



/**
 * Gets a floating point value.
 */
const double PrefsManager::getFloat(const std::string &key, const double fallback) {
    int err;
    sqlite3_stmt *stmt = nullptr;

    // prepare the query
    std::lock_guard<std::mutex> guard(shared->lock);

    SQLite::prepare(shared->db, "SELECT value FROM prefs_number_v1 WHERE key = ?;", &stmt);
    SQLite::bindColumn(stmt, 1, key);

    err = sqlite3_step(stmt);

    // no such row, return default
    if(err == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return fallback;
    } 
    // got a row with this key, get its value
    else if(err == SQLITE_ROW) {
        double temp = 0;
        SQLite::getColumn(stmt, 0, temp);

        sqlite3_finalize(stmt);
        return temp;
    } 
    // DB error
    else {
        throw std::runtime_error(f("Failed to read preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }
}
/**
 * Sets the given unsigned value.
 */
void PrefsManager::setFloat(const std::string &key, const double value) {
    int err;
    sqlite3_stmt *stmt = nullptr;

    std::lock_guard<std::mutex> lg(shared->lock);

    SQLite::prepare(shared->db, "INSERT INTO prefs_number_v1 (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value, modified=CURRENT_TIMESTAMP;", &stmt);
    SQLite::bindColumn(stmt, 1, key);
    SQLite::bindColumn(stmt, 2, value);

    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to write preferences key ({}): {}", err, sqlite3_errmsg(shared->db)));
    }

    // clean up
    sqlite3_finalize(stmt);
}
