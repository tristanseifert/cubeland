#ifndef SHARED_NET_EPPLAYERINFO_H
#define SHARED_NET_EPPLAYERINFO_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <uuid.h>
#include <io/Serialization.h>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace net::message {
/**
 * Player info endpoint message types
 */
enum PlayerInfoMsgType: uint8_t {
    /// client -> server; read request of player info
    kPlayerInfoGet                      = 0x01,
    /// server -> client; player info response
    kPlayerInfoGetResponse              = 0x02,
    /// client -> server; set a player info key
    kPlayerInfoSet                      = 0x03,

    kPlayerInfoTypeMax,
};


/**
 * Client to server request for a player info key
 */
struct PlayerInfoGet {
    std::string key;

    PlayerInfoGet() = default;
    PlayerInfoGet(const std::string &_key) : key(_key) {}

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->key);
        }
};

/**
 * Server response to reading player info
 */
struct PlayerInfoGetReply {
    /// requested key
    std::string key;
    /// whether the key was found for the given player
    bool found;
    /// if found, data for the key
    std::optional<std::vector<std::byte>> data;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->key);
            ar(this->found);
            ar(this->data);
        }
};


/**
 * Client to server request to save a player info key
 */
struct PlayerInfoSet {
    std::string key;
    /// data for the key, or none to remove it
    std::optional<std::vector<std::byte>> data;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->key);
            ar(this->data);
        }
};
}

#endif
