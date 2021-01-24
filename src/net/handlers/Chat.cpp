#include "Chat.h"
#include "net/ServerConnection.h"

#include <net/PacketTypes.h>
#include <net/EPChat.h>

#include <Logging.h>
#include <io/Format.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Checks if we can handle the given packet.
 */
bool Chat::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointChat) return false;
    // it musn't be more than the max value
    if(header.type >= kChatTypeMax) return false;

    return true;
}


/**
 * Handles world info packets.
 */
void Chat::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    PROFILE_SCOPE(Chat);

    switch(header.type) {
        // chat message received
        case kChatMessage:
            this->message(header, payload, payloadLen);
            break;

        // player joined server
        case kChatPlayerJoined:
            this->playerJoined(header, payload, payloadLen);
            break;
        // player left server
        case kChatPlayerLeft:
            this->playerLeft(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid chat packet type: ${:02x}", header.type));
    }
}



/**
 * A new player has joined the server.
 */
void Chat::playerJoined(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // deserialize message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    ChatPlayerJoined joined;
    iArc(joined);

    // TODO: implement
    Logging::debug("Player joined: {} (name '{}')", joined.playerId, joined.displayName);
}

/**
 * A player left the server.
 */
void Chat::playerLeft(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // deserialize message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    ChatPlayerLeft left;
    iArc(left);

    // TODO: implement
    Logging::debug("Player left: {} (reason {})", left.playerId, left.reason);
}



/**
 * New chat message received
 */
void Chat::message(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // deserialize message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    ChatMessage msg;
    iArc(msg);

    // TODO: implement
    Logging::debug("Message: from {}: {}", msg.sender, msg.message);
}
