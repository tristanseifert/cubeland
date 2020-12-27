#include "ResourceManager.h"
#include "PathHelper.h"

#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>

#include <sqlite3.h>

#include <stdexcept>

using namespace io;

ResourceManager *ResourceManager::gShared = nullptr;

/**
 * Opens the default resource library.
 */
ResourceManager::ResourceManager() {
    const auto path = PathHelper::resourcesDir() + "/default.rsrc";
    this->open(path);
}

/**
 * Closes all resource library resources.
 */
ResourceManager::~ResourceManager() {
    if(this->db) {
        sqlite3_close(this->db);
    }
}

/**
 * Opens resource directory at the given path.
 */
void ResourceManager::open(const std::string &path) {
    int err;

    Logging::trace("Loading resources from: {}", path);

    // close previously open db
    if(this->db) {
        sqlite3_close(this->db);
        this->db = nullptr;
    }

    // open in read only mode
    err = sqlite3_open_v2(path.c_str(), &this->db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error("Failed to open resource directory");
    }
}

/**
 * Reads a resource with the given name from the resource directory, if it exists.
 *
 * @note Throws if the resource isn't found. Note that paths do NOT have leading slashes.
 */
void ResourceManager::readResource(const std::string &name, std::vector<unsigned char> &outData) {
    // take lock
    LOCK_GUARD(this->dbLock, LoadResource);

    int err, valueLen;
    sqlite3_stmt *stmt = nullptr;

    // prepare a query and bind the name to it
    const std::string query = "SELECT (content) FROM resources WHERE name = ? LIMIT 1";
    err = sqlite3_prepare_v2(this->db, query.c_str(), query.size(), &stmt, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare query");
    }

    err = sqlite3_bind_text64(stmt, 1, name.c_str(), name.size(), nullptr, SQLITE_UTF8);
    if(err != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind resource name");
    }

    // execute it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to get resource '{}' (check that it exists)", name));
    }

    // get results
    err = sqlite3_column_type(stmt, 0);
    if(err == SQLITE_NULL) {
        // if data is NULL, bail
        sqlite3_finalize(stmt);
        throw std::runtime_error("Resource value is NULL (not allowed)");
    }

    // get the blob value
    auto valuePtr = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(stmt, 0));
    if(valuePtr == nullptr) {
        // data is zero bytes
        outData.clear();
        goto beach;
    }

    valueLen = sqlite3_column_bytes(stmt, 0);
    XASSERT(valueLen > 0, "Invalid BLOB length: {}", valueLen);

    outData.resize(valueLen);
    outData.assign(valuePtr, valuePtr + valueLen);

beach:;
    // clean up
    sqlite3_finalize(stmt);
}
