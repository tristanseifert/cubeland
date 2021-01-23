#ifndef SHARED_NET_EPBLOCKCHANGE_H
#define SHARED_NET_EPBLOCKCHANGE_H

#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <uuid.h>
#include <io/Serialization.h>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

namespace net::message {
/**
 * Message types for block change messages
 */
enum BlockChangeMsgType: uint8_t {
    /// client -> server; changed one or more blocks
    kBlockChangeReport                  = 0x01,
    /// server -> client; broadcast of all changed blocks
    kBlockChangeBroadcast               = 0x02,

    /// client -> server; stop receiving block change notifications for a chunk
    kBlockChangeUnregister              = 0x03,

    kBlockChangeTypeMax,
};


/**
 * Information about a single block that's changed
 */
struct BlockChangeInfo {
    /// position of the chunk that changed
    glm::ivec2 chunkPos;
    /// position of the block, relative to the chunk's origin
    glm::ivec3 blockPos;

    /// block ID to set at this position
    uuids::uuid newId;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->chunkPos);
            ar(this->blockPos);
            ar(this->newId);
        }
};

/**
 * Client to server report of changed blocks
 */
struct BlockChangeReport {
    /// all changed blocks
    std::vector<BlockChangeInfo> changes;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->changes);
        }
};

/**
 * Server broadcast of all changed blocks
 */
struct BlockChangeBroadcast {
    /// all changed blocks
    std::vector<BlockChangeInfo> changes;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->changes);
        }
};

/**
 * Requests that we get no further chunk change notifications.
 */
struct BlockChangeUnregister {
    glm::ivec2 chunkPos;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->chunkPos);
        }
};

}

#endif
