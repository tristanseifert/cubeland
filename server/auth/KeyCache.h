#ifndef AUTH_KEYCACHE_H
#define AUTH_KEYCACHE_H

#include <mutex>
#include <optional>
#include <unordered_map>

#include <uuid.h>

struct sqlite3;

struct evp_pkey_st;

namespace util {
class REST;
}

namespace auth {
/**
 * Loads client keys from the web API and caches them locally.
 *
 * TODO: we should handle expiring keys out of the cache. default TTL of cache keys should be
 * something like 7 days
 */
class KeyCache {
    public:
        static void init() {
            gShared = new KeyCache;
        }
        static void shutdown() {
            delete gShared;
            gShared = nullptr;
        }

        /// returns the public key for the given client
        static evp_pkey_st *get(const uuids::uuid &id) {
            return gShared->getKey(id);
        }

    private:
        KeyCache();
        ~KeyCache();

        evp_pkey_st *getKey(const uuids::uuid &);

        std::optional<std::string> readDbKey(const uuids::uuid &);
        void writeDbKey(const uuids::uuid &, const std::string &);

        evp_pkey_st *decodePem(const std::string &);

    private:
        static KeyCache *gShared;

    private:
        sqlite3 *db = nullptr;

        /// REST handler for accessing Cubeland API
        util::REST *api = nullptr;

        /// mapping of player id -> SSL key
        std::unordered_map<uuids::uuid, evp_pkey_st *> decoded;
        /// lock protecting the list of decoded keys
        std::mutex decodedLock;
};
}

#endif
