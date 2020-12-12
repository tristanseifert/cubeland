/**
 * Provides support for user defaults-type storage.
 *
 * These are automagically persisted to disk.
 */
#ifndef IO_PREFSMANAGER_H
#define IO_PREFSMANAGER_H

#include <memory>
#include <string>
#include <mutex>

namespace libconfig {
class Config;
}

namespace io {
class PrefsManager {
    public:
        static void init() {
            shared = std::make_shared<PrefsManager>();
        }
        static void synchronize() {
            shared->write();
        }

    public:
        static unsigned int getUnsigned(const std::string &key, const unsigned int fallback = 0);
        static void setUnsigned(const std::string &key, const unsigned int value);

        static bool getBool(const std::string &key, const bool fallback = false);
        static void setBool(const std::string &key, const bool value);

    // don't call these
    public:
        PrefsManager();

    private:
        void write();
        void loadDefaults();

    private:
        static std::shared_ptr<PrefsManager> shared;

    private:
        // path to the preferences file on disk
        std::string path;

        // config file
        std::shared_ptr<libconfig::Config> config = nullptr;
        // lock protecting access to the preferences
        std::mutex lock;
};
}

#endif
