#include "Logging.h"
#include "io/PathHelper.h"

#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>

#include "io/ConfigManager.h"

using namespace io;

// shared instance of the logging handler
std::shared_ptr<Logging> Logging::sharedInstance;

/**
 * When logging starts, create the shared logging handler.
 */
void Logging::start() {
    sharedInstance = std::make_unique<Logging>();
}

/**
 * When logging is desired to be stopped, delete the shared logging handler.
 */
void Logging::stop() {
    sharedInstance = nullptr;
}

/**
 * Configure spdlog to log to stdout, file, and/or syslog as configured.
 */
Logging::Logging() {
    using spdlog::async_logger, spdlog::sink_ptr;

    std::vector<sink_ptr> sinks;

    // configure the queue size
    auto queueSz = ConfigManager::getUnsigned("logging.queue.size", 8192);
    queueSz = std::min(1024UL, queueSz);
    auto threads = ConfigManager::getUnsigned("logging.queue.threads", 1);
    threads = std::min(1UL, threads);

    spdlog::init_thread_pool(queueSz, threads);

    // do we want logging to the console?
    if(ConfigManager::getBool("logging.console.enabled", true)) {
        this->configTtyLog(sinks);
    }
    // do we want to log to a file?
    if(ConfigManager::getBool("logging.file.enabled", false)) {
        this->configFileLog(sinks);
    }
    // set up rotating file logger
    this->configRotatingLog(sinks);

    // Warn if no loggers configured
    if(sinks.empty()) {
        std::cerr << "WARNING: No logging sinks configured" << std::endl;
    }

    // create a multi-logger for this
    this->logger = std::make_shared<async_logger>("", sinks.begin(), sinks.end(), 
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    this->logger->set_level(spdlog::level::trace);

    spdlog::set_default_logger(this->logger);
}
/**
 * Cleans up logging.
 */
Logging::~Logging() {
    spdlog::shutdown();
}



/**
 * Configures the console logger.
 */
void Logging::configTtyLog(std::vector<spdlog::sink_ptr> &sinks) {
    // get console logger params
#ifdef NDEBUG
    auto level = this->getLogLevel("logging.console.level", 2);
#else
    auto level = this->getLogLevel("logging.console.level", 0);
#endif
    bool colorize = ConfigManager::getBool("logging.console.colorize", true);

    // Do we want a colorized logger?
    if(colorize) {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_level(level);
        sinks.push_back(sink);
    } 
    // Plain logger
    else {
        auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        sink->set_level(level);
        sinks.push_back(sink);
    }
}

/**
 * Configures the file logger.
 */
void Logging::configFileLog(std::vector<spdlog::sink_ptr> &sinks) {
    using spdlog::sinks::basic_file_sink_mt;

    // get the file logger params
    auto level = getLogLevel("logging.file.level", 2);
    std::string path = ConfigManager::get("logging.file.path", "");
    bool truncate = ConfigManager::getBool("logging.file.truncate", false);

    // create the file logger
    auto file = std::make_shared<basic_file_sink_mt>(path, truncate);
    file->set_level(level);

    sinks.push_back(file);
}

/**
 * Configures the syslog logger.
 */
void Logging::configRotatingLog(std::vector<spdlog::sink_ptr> &sinks) {
    using spdlog::sinks::rotating_file_sink_mt;

    // get the logger params
#ifdef NDEBUG
    const auto level = getLogLevel("logging.rotate.level", 2);
#else
    const auto level = getLogLevel("logging.rotate.level", 1);
#endif
    const auto maxSize = ConfigManager::getUnsigned("logging.rotate.size", 1024 * 200);
    const auto numFiles = ConfigManager::getUnsigned("logging.rotate.files", 10);

    // create the rotating file logger
    const auto logsDir = io::PathHelper::logsDir() + "/main.log";
    auto sink = std::make_shared<rotating_file_sink_mt>(logsDir, maxSize, numFiles, false);
    sink->set_level(level);

    sinks.push_back(sink);
}



/**
 * Converts a numeric log level from the config file into the spdlog value.
 */
spdlog::level::level_enum Logging::getLogLevel(const std::string &path, unsigned long def) {
    static const spdlog::level::level_enum levels[] = {
        spdlog::level::trace,
        spdlog::level::debug,
        spdlog::level::info,
        spdlog::level::warn,
        spdlog::level::err,
        spdlog::level::critical
    };

    // read the preference
    unsigned long numLevel = ConfigManager::getUnsigned(path, def);

    // ensure it's in bounds then check the array
    if(numLevel >= 6) {
        return spdlog::level::trace;
    } else {
        return levels[numLevel];
    }
}

/**
 * Installs a new logging sink.
 */
void Logging::addSink(spdlog::sink_ptr sink) {
    std::lock_guard lg(sharedInstance->loggerLock);

    sharedInstance->logger->sinks().push_back(sink);
}

/**
 * Removes the given sink from the logger.
 */
bool Logging::removeSink(const spdlog::sink_ptr sink) {
    std::lock_guard lg(sharedInstance->loggerLock);

    // check if vector contains this sink
    auto v = sharedInstance->logger->sinks();

    if(std::find(v.begin(), v.end(), sink) == v.end()) {
        return false;
    }

    // if so, remove
    v.erase(std::remove(v.begin(), v.end(), sink), v.end());
    return true;
}

