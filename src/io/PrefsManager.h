/**
 * Provides support for user defaults-type storage.
 *
 * These are automagically persisted to disk.
 */
#ifndef IO_PREFSMANAGER_H
#define IO_PREFSMANAGER_H

#include <memory>
#include <string>
#include <optional>
#include <mutex>

#include <uuid.h>

struct sqlite3;
struct sqlite3_stmt;

namespace io {
class PrefsManager final {
    public:
        static void init() {
            shared = std::make_unique<PrefsManager>();
        }
        static void synchronize() {
            shared->write();
        }

    public:
        static const unsigned int getUnsigned(const std::string &key, const unsigned int fallback = 0);
        static void setUnsigned(const std::string &key, const unsigned int value);

        static const bool getBool(const std::string &key, const bool fallback = false) {
            return (getUnsigned(key, fallback ? 1 : 0) == 1) ? true : false;
        }
        static void setBool(const std::string &key, const bool value) {
            setUnsigned(key, value ? 1 : 0);
        }

        static const std::string getString(const std::string &key, const std::string &fallback = "");
        static void setString(const std::string &key, const std::string &value);

        static const std::optional<uuids::uuid> getUuid(const std::string &key);
        static void setUuid(const std::string &key, const uuids::uuid &value);

    // don't call these
    public:
        PrefsManager();
        ~PrefsManager();

    private:
        void write();

        void initSchema();
        void loadDefaults();

    private:
        static std::unique_ptr<PrefsManager> shared;

    private:
        // path to the preferences file on disk
        std::string path;
        // SQLite db for prefs
        sqlite3 *db = nullptr;
        // lock protecting access to the preferences db
        std::mutex lock;
};
}

#endif
