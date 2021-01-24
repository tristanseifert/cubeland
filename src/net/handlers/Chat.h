#ifndef NET_HANDLER_CHAT_H
#define NET_HANDLER_CHAT_H

#include "net/PacketHandler.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <mutex>
#include <unordered_map>
#include <variant>

#include <uuid.h>

namespace net::handler {
class Chat: public PacketHandler {
    public:
        struct PlayerJoined {
            uuids::uuid id;
            std::string name;
        };
        struct PlayerLeft {
            uuids::uuid id;
        };
        struct Message {
            std::optional<uuids::uuid> from;
            std::string message;
        };

        using EventInfo = std::variant<PlayerJoined, PlayerLeft, Message>;
        using CallbackFunc = std::function<void(const EventInfo &)>;

    public:
        Chat(ServerConnection *_server) : PacketHandler(_server) {};
        virtual ~Chat() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        /// Installs a chat event callback
        uint32_t addCallback(const CallbackFunc &func) {
            std::lock_guard<std::mutex> lg(this->callbacksLock);
            const auto token = this->nextToken++;
            this->callbacks[token] = func;
            return token;
        }
        /// Removes a previously installed callback handler
        void removeCallback(const uint32_t token) {
            std::lock_guard<std::mutex> lg(this->callbacksLock);
            this->callbacks.erase(token);
        }

        void sendMessage(const std::string &msg);

    private:
        void message(const PacketHeader &, const void *, const size_t);

        void playerJoined(const PacketHeader &, const void *, const size_t);
        void playerLeft(const PacketHeader &, const void *, const size_t);

    private:
        std::mutex callbacksLock;
        std::unordered_map<uint32_t, CallbackFunc> callbacks;

        uint32_t nextToken = 1;
};
}

#endif
