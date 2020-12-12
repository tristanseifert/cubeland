#include "PathHelper.h"

#include <Logging.h>

#include <filesystem>

using namespace io;

/**
 * Creates all of our required directories.
 */
void PathHelper::init() {
    // app support directory and related structure
    std::filesystem::create_directories(std::filesystem::path(appDataDir()));

    std::filesystem::create_directories(std::filesystem::path(logsDir()));
    std::filesystem::create_directories(std::filesystem::path(cacheDir()));
}
