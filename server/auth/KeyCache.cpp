#include "KeyCache.h"

#include <Logging.h>
#include <io/PathHelper.h>
#include <util/REST.h>
#include <util/SSLHelpers.h>
#include <util/SQLite.h>

#include <cmrc/cmrc.hpp>

#include <sqlite3.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <filesystem>
#include <stdexcept>

CMRC_DECLARE(server_sql);

using namespace auth;

/// shared instance key cache handler
KeyCache *KeyCache::gShared = nullptr;

/**
 * Initializes the authentication key cache.
 */
KeyCache::KeyCache() {
    int err;

    // path to the cache
    std::filesystem::path path(io::PathHelper::cacheDir());
    path /= "server_keys.sqlite3";

    // open (and create) it
    err = sqlite3_open_v2(path.string().c_str(), &this->db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if(err != SQLITE_OK) {
        throw std::runtime_error(f("Failed to open key cache: {}", err));
    }

    // apply schema if needed
    if(!util::SQLite::tableExists(this->db, "keys_v1")) {
        Logging::debug("Initializing key cache with v1 schema!");

        auto fs = cmrc::server_sql::get_filesystem();
        auto file = fs.open("/keycache_v1.sql");
        std::string schema(file.begin(), file.end());

        err = sqlite3_exec(this->db, schema.c_str(), nullptr, nullptr, nullptr);
        if(err != SQLITE_OK) {
            throw std::runtime_error(f("Failed to write schema ({}): {}", err,
                        sqlite3_errmsg(this->db)));
        }
    }

    // set up the REST handler
#ifdef NDEBUG
    #error "TODO: define API endpoint for prod"
#else
    this->api = new util::REST("http://cubeland-api.test");
#endif
}

/**
 * Tears down all of our resources.
 */
KeyCache::~KeyCache() {
    int err;

    // close db
    err = sqlite3_close(this->db);
    if(err != SQLITE_OK) {
        Logging::warn("Failed to close key cache: {}", err);
    }

    // release keys
    std::lock_guard<std::mutex> lg(this->decodedLock);
    for(const auto &[id, key] : this->decoded) {
        EVP_PKEY_free(key);
    }

    // clean up the remaining stuff
    delete this->api;
}



/**
 * Attempt to retrieve the public key for the given player.
 *
 * We'll first check our cache of already decoded keys and return it, then the on-disk cache, and
 * if neither of those contains the key, we'll make a trip to the REST service and save it to
 * the disk cache.
 *
 * TODO: We need to investigate better locking so that multiple concurrent clients don't race
 */
evp_pkey_st *KeyCache::getKey(const uuids::uuid &id) {
    using namespace rapidjson;

    // try to get it out of the cache
    {
        std::lock_guard<std::mutex> lg(this->decodedLock);
        if(this->decoded.contains(id)) {
            return this->decoded[id];
        }
    }

    // query the database
    auto dbKey = this->readDbKey(id);
    if(dbKey) {
        auto key = this->decodePem(*dbKey);

        std::lock_guard<std::mutex> lg(this->decodedLock);
        this->decoded[id] = key;
        return key;
    }

    // make REST request
    Document response;
    this->api->request(f("/user/{}/pubkey", id), response, false);

    // interpret response
    if(!response["success"].GetBool()) {
        throw std::runtime_error("REST request failed");
    }

    // store the key in our db and store it for later
    const auto apiKey = response["key"].GetString();
    auto key = this->decodePem(apiKey);

    this->writeDbKey(id, apiKey);

    std::lock_guard<std::mutex> lg(this->decodedLock);
    this->decoded[id] = key;
    return key;
}

/**
 * Searches the key cache for a key with the given UUID. If found, we return the PEM-encoded public
 * key string.
 */
std::optional<std::string> KeyCache::readDbKey(const uuids::uuid &id) {
    using namespace util;

    int err;
    sqlite3_stmt *stmt = nullptr;

    // prepare query and bind the key
    SQLite::prepare(this->db, "SELECT id,pubkey FROM keys_v1 WHERE uuid=?", &stmt);
    SQLite::bindColumn(stmt, 1, id);

    // execute it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("failed to step ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    if(err != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    // extract the key string
    std::string keyStr;
    SQLite::getColumn(stmt, 1, keyStr);

    sqlite3_finalize(stmt);
    return keyStr;
}

/**
 * Writes a new key to the cache.
 */
void KeyCache::writeDbKey(const uuids::uuid &id, const std::string &keyStr) {
    using namespace util;

    int err;
    sqlite3_stmt *stmt = nullptr;

    // prepare query and bind the key
    SQLite::prepare(this->db, "INSERT INTO keys_v1 (uuid,pubkey) VALUES (?, ?)", &stmt);
    SQLite::bindColumn(stmt, 1, id);
    SQLite::bindColumn(stmt, 2, keyStr);

    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("failed to step ({}): {}", err, sqlite3_errmsg(this->db)));
    }

    sqlite3_finalize(stmt);
}


/**
 * Decodes a PEM encoded key.
 */
evp_pkey_st *KeyCache::decodePem(const std::string &keyStr) {
    int err;

    // load the key string into a BIO
    BIO *bio = BIO_new(BIO_s_mem());
    XASSERT(bio, "Failed to allocate mem BIO");

    auto readPtr = keyStr.data();
    size_t toWrite = keyStr.size();

    while(toWrite > 0) {
        err = BIO_write(bio, readPtr, toWrite);

        if(err <= 0) {
            throw std::runtime_error(f("BIO_write failed: {}", err));
        }

        toWrite -= err;
        readPtr += err;
    }

    // then decode
    auto key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    if(!key) {
        BIO_free(bio);
        throw std::runtime_error(f("Failed to load key: {}", util::SSLHelpers::getErrorStr()));
    }

    BIO_free(bio);
    return key;
}
