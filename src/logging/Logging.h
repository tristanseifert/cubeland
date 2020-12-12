/**
 * Central logger for the rest of the application. Automagically handles
 * sending messages to the correct outputs.
 */
#ifndef LOGGING_H
#define LOGGING_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include <spdlog/spdlog.h>
#include <fmt/format.h>


class Logging {
    public:
        static void start();
        static void stop();

        // you shouldn't call this lol
        Logging();
        virtual ~Logging();

    public:
        /// Trace level logging
        template<typename... Args> static inline void trace(std::string fmt, 
                const Args &... args) {
            spdlog::trace(fmt, args...);
        }
        
        /// Debug level logging
        template<typename... Args> static inline void debug(std::string fmt, 
                const Args &... args) {
            spdlog::debug(fmt, args...);
        }
        
        /// Info level logging
        template<typename... Args> static inline void info(std::string fmt, 
                const Args &... args) {
            spdlog::info(fmt, args...);
        }
        
        /// Warning level logging
        template<typename... Args> static inline void warn(std::string fmt, 
                const Args &... args) {
            spdlog::warn(fmt, args...);
        }
        
        /// Error level logging
        template<typename... Args> static inline void error(std::string fmt, 
                const Args &... args) {
            spdlog::error(fmt, args...);
        }
        
        /// Critical level logging
        template<typename... Args> static inline void crit(std::string fmt, 
                const Args &... args) {
            spdlog::critical(fmt, args...);
        }

        /**
         * Handles a failed assertion. This will log the message out, but
         * not terminate; the XASSERT() macro has a call to std::abort().
         */
        template<typename... Args>
        static bool assertFailed(const char *expr, const char *file, 
                int line, const std::string msg, const Args &... args) {
            auto fmtMsg = fmt::format(msg, args...);
            crit("ASSERTION FAILURE ({}:{}) {} {}", file, line, expr, 
                    fmtMsg);
            // required to flush the log queue before terminating
            spdlog::shutdown();
            return true;
        }

        /// Adds a new logging sink.
        static void addSink(spdlog::sink_ptr sink);
        /// Removes the specified sink, if present.
        static bool removeSink(const spdlog::sink_ptr sink);

    private:
        void configTtyLog(std::vector<spdlog::sink_ptr> &);
        void configFileLog(std::vector<spdlog::sink_ptr> &);
        void configRotatingLog(std::vector<spdlog::sink_ptr> &);

        static spdlog::level::level_enum getLogLevel(const std::string &, unsigned long);

    private:
        static std::shared_ptr<Logging> sharedInstance;

        std::shared_ptr<spdlog::logger> logger;
        std::mutex loggerLock;
};

// include this below here, as it depends on calling into the logger
#include "LoggingAssert.h"

#endif
