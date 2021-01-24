#ifndef SHARED_NET_EPCHAT_H
#define SHARED_NET_EPCHAT_H

#include <chrono>
#include <optional>
#include <string>

#include <uuid.h>
#include <io/Serialization.h>

#include <cereal/access.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>

namespace net::message {
/**
 * Message types for the chat endpoint
 */
enum ChatMsgType: uint8_t {
    /// client -> server; message sent by client
    kChatPlayerMessage                  = 0x01,
    /// server -> client; message to clients
    kChatMessage                        = 0x02,
    /// server -> client; a player joined
    kChatPlayerJoined                   = 0x03,
    /// server -> client; a player left
    kChatPlayerLeft                     = 0x04,

    kChatTypeMax,
};


/**
 * Messages sent by clients.
 */
struct ChatPlayerMessage {
    /// message content. sent to all players
    std::string message;

    ChatPlayerMessage() = default;
    ChatPlayerMessage(const std::string &_content) : message(_content) {}

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->message);
        }
};

/**
 * Packets of this type contain messages that were sent by clients (or the server) and should be
 * displayed on the client.
 */
struct ChatMessage {
    /// sender of the message; if none, it was a global (wall) message
    std::optional<uuids::uuid> sender;
    /// message content
    std::string message;
    /// timestamp (when the message was originally sent)
    std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now();

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->sender);
            ar(this->message);
            ar(this->time);
        }
};

/**
 * Messages indicating a player has joined the server. Clients should build up a list of players
 * from these messages to tag messages.
 */
struct ChatPlayerJoined {
    /// ID of the player
    uuids::uuid playerId;
    /// player-set display name
    std::string displayName;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->playerId);
            ar(this->displayName);
        }
};

/**
 * A player has disconnected from the server. This can include an optional reason.
 */
struct ChatPlayerLeft {
    enum class Reason {
        /// generic disconnection reason
        Unknown,
        /// user quit the game
        Quit,
    };

    /// ID of the player, from join message
    uuids::uuid playerId;
    /// disconnection reason
    Reason reason = Reason::Unknown;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->playerId);
            ar(this->reason);
        }
};

}

#endif
