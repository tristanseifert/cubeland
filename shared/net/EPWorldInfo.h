#ifndef SHARED_NET_EPWORLDINFO_H
#define SHARED_NET_EPWORLDINFO_H

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
 * World info endpoint message types
 */
enum WorldInfoMsgType: uint8_t {
    /// client -> server; read request of world info
    kWorldInfoGet                       = 0x01,
    /// server -> client; world info response
    kWorldInfoGetResponse               = 0x02,

    kWorldInfoTypeMax,
};


/**
 * Client to server request for a world info key
 */
struct WorldInfoGet {
    /// key (name of the world info)
    std::string key;

    WorldInfoGet() = default;
    WorldInfoGet(const std::string &_key) : key(_key) {}

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->key);
        }
};

/**
 * Server response to reading world info
 */
struct WorldInfoGetReply {
    /// key of the world info
    std::string key;
    /// whether the key was found in the world
    bool found;
    /// if found, data for the key. may be a 0 byte vector
    std::optional<std::vector<std::byte>> data;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->key);
            ar(this->found);
            ar(this->data);
        }
};

}

#endif
