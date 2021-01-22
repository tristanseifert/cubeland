#ifndef SHARED_NET_EPCHUNK_H
#define SHARED_NET_EPCHUNK_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <uuid.h>
#include <io/Serialization.h>
#include <glm/vec2.hpp>

#include <cereal/access.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

namespace net::message {
/**
 * Chunk data message types
 */
enum ChunkMsgType: uint8_t {
    /// client -> server; request chunk data
    kChunkGet                           = 0x01,
    /// server -> client; chunk slice
    kChunkSliceData                     = 0x02,
    /// server -> client; chunk transfer completed
    kChunkCompletion                    = 0x03,

    kChunkTypeMax,
};

/**
 * Client to server request to load a chunk.
 *
 * Chunks are sent slice by slice -- not necessarily in order -- until all slices with data have
 * been transmitted. Then, a final completion message is sent. Because TCP ensures order, this
 * means we'll have all the slices processed at that time.
 */
struct ChunkGet {
    /// position of the chunk
    glm::ivec2 chunkPos;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->chunkPos);
        }
};

/**
 * Data for a single slice.
 */
struct ChunkSliceData {
    /// position of the chunk to which this slice belongs
    glm::ivec2 chunkPos;
    /// Y level of the slice
    uint16_t y;

    /// mapping of UUID to integer value stored in here
    std::unordered_map<uuids::uuid, uint16_t> typeMap;

    /// an LZ4 compressed 256x256 array of 16-bit values, in Z-major order
    std::vector<std::byte> data;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->chunkPos);
            ar(this->y);
            ar(this->typeMap);
            ar(this->data);
        }
};

/**
 * Message sent by the server to indicate an entire chunk worth of slice data has been sent.
 */
struct ChunkCompletion {
    /// position of the completed chunk
    glm::ivec2 chunkPos;
    /// total number of Y slices in the chunk
    uint16_t numSlices;

    /// chunk metadata
    std::unordered_map<std::string, world::MetaValue> meta;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->chunkPos);
            ar(this->numSlices);
            ar(this->meta);
        }
};

}

#endif
