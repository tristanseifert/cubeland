#ifndef NET_HANDLER_PLAYERINFO_H
#define NET_HANDLER_PLAYERINFO_H

#include "net/PacketHandler.h"

#include <cstddef>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace net::handler {

class PlayerInfo: public PacketHandler {
    public:
        PlayerInfo(ServerConnection *_server);
        virtual ~PlayerInfo();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        std::future<std::optional<std::vector<std::byte>>> get(const std::string &key);
        void set(const std::string &key, const std::vector<std::byte> &value);

    private:
        void receivedKey(const PacketHeader &, const void *, const size_t);

    private:
        /// lock over the promises list
        std::mutex requestsLock;
        /// outstanding requests
        std::unordered_map<std::string, std::promise<std::optional<std::vector<std::byte>>>> requests;
};
}

#endif
