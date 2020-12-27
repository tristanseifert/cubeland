/**
 * Handles loading resources from a resource bundle.
 */
#ifndef IO_RESOURCEMANAGER_H
#define IO_RESOURCEMANAGER_H

#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

namespace io {
class ResourceManager {
    public:
        /// Initializes the resource manager.
        static void init() {
            gShared = new ResourceManager;
        }
        /// Shuts down the resource manager
        static void shutdown() {
            delete gShared;
            gShared = nullptr;
        }

        /// Retrieves the data for the given named resource
        static void get(const std::string &name, std::vector<unsigned char> &outData) {
            gShared->readResource(name, outData);
        }

    private:
        ResourceManager();
        ~ResourceManager();

        void open(const std::string &path);

        void readResource(const std::string &name, std::vector<unsigned char> &outData);

    private:
        /// Shared instance resource manager
        static ResourceManager *gShared;

    private:
        /// lock protecting access to the resource directory
        std::mutex dbLock;
        /// currently loaded resource directory
        sqlite3 *db = nullptr;
};
}

#endif
