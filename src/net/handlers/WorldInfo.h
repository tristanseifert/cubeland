#ifndef NET_HANDLER_WORLDINFO_H
#define NET_HANDLER_WORLDINFO_H

#include "net/PacketHandler.h"

#include <cstddef>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace net::handler {

class WorldInfo: public PacketHandler {
    public:
        WorldInfo(ServerConnection *_server);
        virtual ~WorldInfo();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        std::future<std::optional<std::vector<std::byte>>> get(const std::string &key);

    private:
        void receivedKey(const PacketHeader &, const void *, const size_t);

    private:
        /// lock over the promises list
        std::mutex requestsLock;
        /// outstanding requests
        std::unordered_map<std::string, std::promise<std::optional<std::vector<std::byte>>>> requests;

        /// lock protecting world info cache
        std::mutex cacheLock;
        /// cache of world info keys -> values
        std::unordered_map<std::string, std::vector<std::byte>> cache;
};
}

#endif
