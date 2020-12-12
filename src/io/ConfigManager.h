/**
 * Provides access to the configuration.
 *
 * Once the config is loaded during startup, any code in the server may call
 * the shared instance and request a config value by its keypath. Values can be
 * retrieved in most primitive types.
 */
#ifndef IO_CONFIGMANAGER_H
#define IO_CONFIGMANAGER_H

#include <string>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <cmath>

#include <libconfig.h++>

#include <sys/time.h>

namespace io {
class ConfigManager {
    public:
        static void readConfig(const std::string &path, bool load = true);

    // you should not call this!
        ConfigManager(const std::string &path, bool load);

    public:
        static const bool getBool(const std::string &path, const bool fallback = false) {
            try {
                return sharedInstance()->getPrimitive(path);
            } catch (KeyException &) {
                return fallback;
            }
        }
        static const long getNumber(const std::string &path, const long fallback = -1) {
            try {
                return sharedInstance()->getPrimitive(path);
            } catch (KeyException &) {
                return fallback;
            }
        }
        static const unsigned long getUnsigned(const std::string &path, const unsigned long fallback = 0) {
            try {
                return sharedInstance()->getPrimitive(path);
            } catch (KeyException &) {
                return fallback;
            }
        }
        static const double getDouble(const std::string &path, const double fallback = 0) {
            try {
                return sharedInstance()->getPrimitive(path);
            } catch (KeyException &) {
                return fallback;
            }
        }
        static const std::string get(const std::string &path, const std::string &fallback = "") {
            try {
                return sharedInstance()->getPrimitive(path);
            } catch (KeyException &) {
                return fallback;
            }
        }

        static const struct timeval getTimeval(const std::string &path, const double fallback = 2) {
            // read double and separate into fraction and whole parts
            double fraction, whole;
            double value = getDouble(path, fallback);
            fraction = modf(value, &whole);

            // convert to timeval
            struct timeval tv;

            tv.tv_sec = whole;
            tv.tv_usec = (fraction * 1000 * 1000);

            return tv;
        }

    private:
        static std::shared_ptr<ConfigManager> sharedInstance();
        
        libconfig::Setting &getPrimitive(const std::string &path);

    private:
        std::mutex cfgLock;
        std::unique_ptr<libconfig::Config> cfg = nullptr;

    // Error types
    public:
        // Failed to read/write config
        class IOException : public std::runtime_error {
            friend class ConfigManager;
            private:
                IOException(const std::string &what) : std::runtime_error(what) {}
        };
        
        // Could not find or convert key
        class KeyException : public std::runtime_error {
            friend class ConfigManager;
            private:
                KeyException(const std::string &what) : std::runtime_error(what) {}
        };

        // Failed to parse config
        class ParseException : public std::runtime_error {
            friend class ConfigManager;
            private:
                ParseException(const std::string &what, int line) : 
                    std::runtime_error(what), line(line) {}
                int line = -1;

            public:
                int getLine() const {
                    return this->line;
                }
        };

};
}

#endif
