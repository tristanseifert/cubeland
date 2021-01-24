#include "Chat.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include <net/PacketTypes.h>
#include <net/EPChat.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

std::atomic_bool Chat::broadcastRun;
std::unique_ptr<std::thread> Chat::broadcastThread;
moodycamel::BlockingConcurrentQueue<Chat::BroadcastItem> Chat::broadcastQueue;

/**
 * We handle all world info endpoint packets.
 */
bool Chat::canHandlePacket(const PacketHeader &header) {
    // it must be the world info endpoint
    if(header.endpoint != kEndpointChat) return false;
    // it musn't be more than the max value
    if(header.type >= kChatTypeMax) return false;

    return true;
}

/**
 * Handles world info packets
 */
void Chat::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        default:
            throw std::runtime_error(f("Invalid chat packet type: ${:02x}", header.type));
    }
}



/**
 * Starts the broadcasting thread.
 */
void Chat::startBroadcaster(net::Listener *list) {
    broadcastRun = true;
    broadcastThread = std::make_unique<std::thread>(std::bind(&Chat::broadcasterMain, list));
}

/**
 * Terminates the broadcasting thread.
 */
void Chat::stopBroadcaster() {
    // push some dummy work so the thread wakes
    broadcastRun = false;
    broadcastQueue.enqueue(BroadcastItem());

    // wait for it to terminate
    broadcastThread->join();
    broadcastThread = nullptr;
}

/**
 * Main loop for the broadcast thread
 */
void Chat::broadcasterMain(net::Listener *listener) {
    util::Thread::setName("Chat Broadcaster");

    BroadcastItem item;

    // try to receive work updates
    while(broadcastRun) {
        broadcastQueue.wait_dequeue(item);

        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            // send messages to all clients
            if constexpr (std::is_same_v<T, Message>) {
                broadcastMessage(listener, arg);
            }
            // a player joined the server
            else if constexpr (std::is_same_v<T, PlayerJoined>) {
                broadcastPlayerJoined(listener, arg);
            }
            // a player left the server
            else if constexpr (std::is_same_v<T, PlayerLeft>) {
                broadcastPlayerLeft(listener, arg);
            }
            // everything else is treated as a no-op
            else {}
        }, item);
    }

    // clean up
}

/**
 * Serializes a message and sends it to all connected clients.
 */
void Chat::broadcastMessage(net::Listener *listener, const Message &_msg) {
    // build and serialize the message
    ChatMessage msg;

    msg.sender = _msg.from;
    msg.message = _msg.content;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(msg);

    const auto str = oStream.str();
    broadcast(listener, str, kChatMessage);
}

/**
 * Notifies clients that a new player has joined.
 */
void Chat::broadcastPlayerJoined(net::Listener *listener, const PlayerJoined &_msg) {
    // build and serialize message
    ChatPlayerJoined joined;
    joined.playerId = _msg.id;
    joined.displayName = _msg.name;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(joined);

    const auto str = oStream.str();
    broadcast(listener, str, kChatPlayerJoined);
}

/**
 * Notifies clients that a player left the server.
 */
void Chat::broadcastPlayerLeft(net::Listener *listener, const PlayerLeft &_msg) {
    // build and serialize message
    ChatPlayerJoined left;
    left.playerId = _msg.id;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(left);

    const auto str = oStream.str();
    broadcast(listener, str, kChatPlayerLeft);
}

/**
 * Broadcasts a message to all clients.
 */
void Chat::broadcast(net::Listener *listener, const std::string &str, const uint8_t type) {
    listener->forEach([&, type, str](auto &client) {
        // ignore unauthenticated clients
        auto clientId = client->getClientId();
        if(!clientId) return;

        client->writePacket(kEndpointChat, type, str);
    });
}
