#include "io/ConfigManager.h"
#include "logging/Logging.h"

#include <version.h>

#include <iostream>
#include <string>

/**
 * Config options as read from command line
 */
static struct {
    // config file path
    std::string configPath = "./cubeland.conf";
} cmdline;

/**
 * Reads configuration from the given file.
 */
static int ReadConfig(const std::string &path, bool ioErrorFatal = true) {
    try {
        io::ConfigManager::readConfig(path);
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

    // set up platform stuff, logging, read config
    err = ReadConfig(cmdline.configPath, false);
    if(err != 0) {
        std::cerr << "Failed to load configuration: " << err << std::endl;
    }

    Logging::start();
    Logging::info("Cubeland ({}) starting", gVERSION);

    // set up the UI layer

    // start main loop

    // tear down UI

    // last, stop logging
    Logging::stop();
}
