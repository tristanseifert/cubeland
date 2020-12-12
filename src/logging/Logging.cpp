#include "Logging.h"

#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>

#include <syslog.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
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
    Logging::debug("Initialized logging");
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
    // do we want to log to syslog?
    if(ConfigManager::getBool("logging.syslog.enabled", false)) {
        this->configSyslog(sinks);
    }

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
#ifdef DEBUG
    auto level = this->getLogLevel("logging.console.level", 0);
#else
    auto level = this->getLogLevel("logging.console.level", 2);
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
void Logging::configSyslog(std::vector<spdlog::sink_ptr> &sinks) {
    using spdlog::sinks::syslog_sink_mt;

    // get syslog params
    auto level = getLogLevel("logging.syslog.level", 2);
    std::string ident = ConfigManager::get("logging.syslog.ident", "lichtenstein_server");
    int facility = getSyslogFacility("logging.syslog.facility", LOG_LOCAL0);

    // create syslog logger
    auto syslog = std::make_shared<syslog_sink_mt>(ident, LOG_PID | LOG_NDELAY, 
            facility, false);
    syslog->set_level(level);

    sinks.push_back(syslog);
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
 * Maps the string names for syslog facilities to the appropriate integer
 * constant value.
 */
int Logging::getSyslogFacility(const std::string &path, int def) {
    static const std::unordered_map<std::string, int> facilities = {
        {"auth", LOG_AUTH},
        {"authpriv", LOG_AUTHPRIV},
        {"cron", LOG_CRON},
        {"daemon", LOG_DAEMON},
        {"ftp", LOG_FTP},
        {"local0", LOG_LOCAL0},
        {"local1", LOG_LOCAL1},
        {"local2", LOG_LOCAL2},
        {"local3", LOG_LOCAL3},
        {"local4", LOG_LOCAL4},
        {"local5", LOG_LOCAL5},
        {"local6", LOG_LOCAL6},
        {"local7", LOG_LOCAL7},
        {"lpr", LOG_LPR},
        {"mail", LOG_MAIL},
        {"news", LOG_NEWS},
        {"syslog", LOG_SYSLOG},
        {"user", LOG_USER},
        {"uucp", LOG_UUCP},
    };

    // check if the input is in the map
    auto it = facilities.find(path);

    if(it == facilities.end()) {
        return def;
    } else {
        return it->second;
    }
}




