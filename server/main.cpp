#include <io/ConfigManager.h>
#include <io/PathHelper.h>
#include <io/Format.h>
#include <logging/Logging.h>

#include <world/FileWorldReader.h>
#include <world/generators/Terrain.h>
#include <world/WorldSource.h>

#include "auth/KeyCache.h"
#include "net/Listener.h"

#include <version.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <unistd.h>
#include <signal.h>

#include <lyra/lyra.hpp>

static world::WorldSource *LoadWorld(const std::string &path);

/// main loop run flag. cleared by the Ctrl+C signal handler
static std::atomic_bool keepRunning = true;

/**
 * Config options as read from command line
 */
static struct {
    // print usage and exit
    bool help = false;
    // config file path
    std::string configPath = io::PathHelper::appDataDir() + "/server.conf";
} cmdline;

/**
 * Signal handler. This handler is invoked for the following signals to enable us to do a clean
 * shut-down:
 *
 * - SIGINT
 */
static void CtrlCHandler(int sig) {
    Logging::info("Caught signal {}; shutting down", sig);
    keepRunning = false;
}

/**
 * Parse the command line.
 *
 * @return 0 if program should continue, positive to exit (but return 0), negative if error.
 */
static int ParseCommandLine(const int argc, const char **argv) {
    auto cli = lyra::cli()
        | lyra::opt(cmdline.configPath, "config")
          ["-c"]["--config"]
          (f("Path to a file from which server configuration is read. (Default: {})", cmdline.configPath))
        | lyra::help(cmdline.help);
    auto result = cli.parse( { argc, argv } );
    if(!result) {
        std::cerr << "Failed to parse command line: " << result.errorMessage() << std::endl;
        return -1;
    }

    if(cmdline.help) {
        std::cout << cli;
        return 1;
    }

    return 0;
}

/**
 * Reads configuration from the given file.
 */
static int ReadConfig(const std::string &path, bool ioErrorFatal = true) {
    // check if it exists
    bool load = false;

    try {
        load = std::filesystem::exists(std::filesystem::path(path));
    } catch (std::exception &e) {  }

    if(!load) {
        std::cerr << "Failed to load config file from '" << path << "'" << std::endl;
        return -1;
    }

    // then try to load
    try {
        io::ConfigManager::readConfig(path, load);
    } catch (io::ConfigManager::IOException &e) {
        if(ioErrorFatal) {
            std::cerr << "Failed to read config from '" << path << "' (" << e.what() << ")" 
                      << std::endl;
            return -1;
        }
    } catch(io::ConfigManager::ParseException &e) { 
        std::cerr << "Parse error on line " << e.getLine() << " of config: " << e.what() 
                  << std::endl;
        return -1;
    }

    return 0;
}

/**
 * Entry point for the server.
 */
int main(int argc, const char **argv) {
    int err;

    // parse the command line options, load config
    err = ParseCommandLine(argc, argv);
    if(err < 0) {
        return err;
    } else if(err > 0) {
        return 0;
    }

    io::PathHelper::init();

    err = ReadConfig(cmdline.configPath, false);
    if(err != 0) {
        std::cerr << "Failed to load configuration: " << err << std::endl;
        return err;
    }

    Logging::start();
    Logging::info("Cubeland Server {} starting", gVERSION_TAG);

    auth::KeyCache::init();

    // open the world and start up the server
    const auto worldPath = io::ConfigManager::get("world.path", "");

    auto source = LoadWorld(worldPath);
    auto listener = new net::Listener(source);

    // we really, really do not care about SIGPIPE signals
    signal(SIGPIPE, SIG_IGN);

    // server run loop; catch Ctrl+C so we can exit
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = CtrlCHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, nullptr);

    while(keepRunning) {
        pause();
    }

    source->flushDirtyChunksSync();

    // clean up
    Logging::info("Stopping server...");

    delete listener;
    delete source;

    auth::KeyCache::shutdown();

    Logging::stop();
}

/**
 * Sets up a world source with the server's world and generator.
 */
static world::WorldSource *LoadWorld(const std::string &path) {
    // open world file
    auto file = std::make_shared<world::FileWorldReader>(path, true);

    // set up the appropriate generator
    auto seedProm = file->getWorldInfo("generator.seed");
    auto seedData = seedProm.get_future().get();

    int32_t seed = io::ConfigManager::getUnsigned("world.seed", 420);
    if(!seedData.empty()) {
        // seed is stored as a string
        const std::string seedStr(seedData.begin(), seedData.end());
        seed = stoi(seedStr);
    } else {
        Logging::warn("Failed to load seed for world {}; using config value (world.seed) ${:X}",
                path, seed);
    }

    auto gen = std::make_shared<world::Terrain>(seed);

    // create world source
    const auto numWorkers = io::ConfigManager::getUnsigned("world.sourceWorkThreads", 4);
    auto source = new world::WorldSource(file, gen, numWorkers);

    return source;
}
