#include "PrefsManager.h"
#include "PathHelper.h"
#include "Format.h"

#include <Logging.h>

#include <libconfig.h++>

#include <iostream>
#include <filesystem>

using namespace io;

std::shared_ptr<PrefsManager> PrefsManager::shared = nullptr;

/**
 * Sets the default preferences path.
 */
PrefsManager::PrefsManager() {
    this->path = PathHelper::appDataDir() + "/preferences.conf";

    // create the config object backing our storage
    this->config = std::make_shared<libconfig::Config>();
    this->config->setAutoConvert(true);

    // load from disk if file exists
    if(std::filesystem::exists(std::filesystem::path(this->path))) {
        try {
            this->config->readFile(this->path.c_str());
        } catch (const libconfig::ParseException &parse) {
            auto what = f("(line {}) {}; {}", parse.getLine(), parse.what(), parse.getError());
            std::cerr << "Failed to parse preferences file: " << what << std::endl;
        } catch (const libconfig::FileIOException &e) {
            std::cerr << "Failed to load preferences: " << e.what() << std::endl;
        }
    } 

    // apply the defaults for any missing prefs
    this->loadDefaults();
}

/**
 * Loads default settings.
 */
void PrefsManager::loadDefaults() {
    // create the "window" section
    if(!this->config->exists("window")) {
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
    }
}

/**
 * Writes the current set of preferences out to disk.
 */
void PrefsManager::write() {
    try {
        this->config->writeFile(this->path.c_str());
    } catch (const libconfig::FileIOException &e) {
        Logging::error("Failed to write prefs to '{}': {}", this->path, e.what());
    }
}

/**
 * Gets an unsigned integer value.
 */
unsigned int PrefsManager::getUnsigned(const std::string &key, const unsigned int fallback) {
    unsigned int value;

    std::lock_guard<std::mutex> guard(shared->lock);
    if(shared->config->lookupValue(key, value)) {
        return value;
    }
    return fallback;
}
/**
 * Sets the given unsigned value.
 */
void PrefsManager::setUnsigned(const std::string &key, const unsigned int value) {
    if(shared->config->exists(key)) {
        shared->config->lookup(key) = (long) value;
    }
}

/**
 * Gets a boolean value.
 */
bool PrefsManager::getBool(const std::string &key, const bool fallback) {
    bool value;

    std::lock_guard<std::mutex> guard(shared->lock);
    if(shared->config->lookupValue(key, value)) {
        return value;
    }
    return fallback;
}
/**
 * Sets the given boolean value.
 */
void PrefsManager::setBool(const std::string &key, const bool value) {
    if(shared->config->exists(key)) {
        shared->config->lookup(key) = value;
    }
}

