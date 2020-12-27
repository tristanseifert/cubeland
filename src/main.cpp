#include "io/ConfigManager.h"
#include "io/PrefsManager.h"
#include "io/PathHelper.h"
#include "io/ResourceManager.h"
#include "io/Format.h"
#include "logging/Logging.h"
#include "gui/MainWindow.h"

#include <version.h>

#include <iostream>
#include <string>
#include <memory>
#include <filesystem>
#include <cstdlib>

#include <lyra/lyra.hpp>

#include <SDL.h>

/// Main window
static std::shared_ptr<gui::MainWindow> window = nullptr;

/**
 * Config options as read from command line
 */
static struct {
    // print usage and exit
    bool help = false;
    // config file path
    std::string configPath = io::PathHelper::appDataDir() + "/cubeland.conf";
} cmdline;

/**
 * Parse the command line.
 *
 * @return 0 if program should continue, positive to exit (but return 0), negative if error.
 */
static int ParseCommandLine(const int argc, const char **argv) {
    auto cli = lyra::cli()
        | lyra::opt(cmdline.configPath, "config")
          ["-c"]["--config"]
          (f("Path to a file from which app configuration is read. (Default: {})", cmdline.configPath))
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
#ifndef NDEBUG
        std::cerr << "No config file at '" << path << "'" << std::endl;
#endif
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
 * Program entry point
 */
int main(int argc, const char **argv) {
    int err;

    // parse the command line options
    err = ParseCommandLine(argc, argv);
    if(err < 0) {
        return err;
    } else if(err > 0) {
        return 0;
    }

    // set up platform specifics, read config and prefs, then begin logger
    io::PathHelper::init();

    err = ReadConfig(cmdline.configPath, false);
    if(err != 0) {
        std::cerr << "Failed to load configuration: " << err << std::endl;
    }
    io::PrefsManager::init();

    Logging::start();
    Logging::info("Cubeland {} (commit {}) starting", gVERSION, gVERSION_HASH);

    io::ResourceManager::init();

    // initialize SDL
    err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    XASSERT(err == 0, "Failed to initialize SDL ({}): {}", err, SDL_GetError());

    atexit(SDL_Quit);

    // set up the UI layer
    window = std::make_shared<gui::MainWindow>();

    // start main loop
    window->show();

    err = window->run();
    Logging::debug("MainWindow::run() returned: {}", err);

    // tear down UI and other systems
    window = nullptr;

    io::ResourceManager::shutdown();
    io::PrefsManager::synchronize();

    // last, stop logging
    Logging::stop();
}
