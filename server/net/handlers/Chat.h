#ifndef NET_HANDLER_CHAT_H
#define NET_HANDLER_CHAT_H

#include "net/PacketHandler.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

#include <uuid.h>

#include <blockingconcurrentqueue.h>
#include <cereal/access.hpp>

namespace net {
class Listener;
}

namespace net::handler {
/**
 * Receives chat messages from clients, and with a static worker thread, reflect them back to all
 * other connected clients, as well as notify clients when they are joining/leaving the server.
 */
class Chat: public PacketHandler {
    public:
        Chat(ListenerClient *_client) : PacketHandler(_client) {};
        virtual ~Chat() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        /// messages to send to clients
        struct Message {
            uuids::uuid from;
            std::string content;
        };
        /// player joined event
        struct PlayerJoined {
            uuids::uuid id;
            std::string name;

            PlayerJoined(const uuids::uuid &_id, const std::string &_name) : id(_id),name(_name) {}
        };
        /// player disconnected or left
        struct PlayerLeft {
            uuids::uuid id;

            PlayerLeft(const uuids::uuid &_id) : id(_id) {}
        };

        /// wrap up the message types
        using BroadcastItem = std::variant<std::monostate, Message, PlayerJoined, PlayerLeft>;

    public:
        static void playerJoined(const uuids::uuid &id, const std::string &name) {
            broadcastQueue.enqueue(PlayerJoined(id, name));
        }
        static void playerLeft(const uuids::uuid &id) {
            broadcastQueue.enqueue(PlayerLeft(id));
        }

        static void startBroadcaster(net::Listener *);
        static void stopBroadcaster();

    private:
        static void broadcasterMain(net::Listener *);
        static void broadcastMessage(net::Listener *, const Message &);
        static void broadcastPlayerJoined(net::Listener *, const PlayerJoined &);
        static void broadcastPlayerLeft(net::Listener *, const PlayerLeft &);

        static void broadcast(net::Listener *, const std::string &, const uint8_t);

    private:
        void playerMessage(const PacketHeader &, const void *, const size_t);

    private:
        static std::unique_ptr<std::thread> broadcastThread;
        static std::atomic_bool broadcastRun;
        static moodycamel::BlockingConcurrentQueue<BroadcastItem> broadcastQueue;
};
}

#endif
