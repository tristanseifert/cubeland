/**
 * Allows to easily get paths to specific user directories, temporary directories, and so on.
 */
#ifndef IO_PATHHELPER_H
#define IO_PATHHELPER_H

#include <string>

namespace io {
class PathHelper {
    public:
        static void init();

        static std::string appDataDir();
        static std::string resourcesDir();

    public:
        static std::string logsDir() {
            return appDataDir() + "/Logs";
        }
        static std::string cacheDir() {
            return appDataDir() + "/Caches";
        }
};
}

#endif
