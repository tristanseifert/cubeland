/**
 * In-memory representation of a chunk, as well as per-chunk metadata.
 *
 * For simplicity, the chunk is also where per-block metadata is stored, when in memory. These
 * per block metadata use integer keys, rather than string keys; a separate map establishes the
 * mapping of chunk local integers to the global string values.
 */
#ifndef WORLD_CHUNK_CHUNK_H
#define WORLD_CHUNK_CHUNK_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <array>
#include <unordered_map>
#include <string>
#include <variant>
#include <tuple>
#include <vector>

#include <glm/vec2.hpp>

namespace world {

struct ChunkSlice;
struct ChunkSliceTypeMap;

/// Types that may be held as chunk metadata values
using MetaValue = std::variant<std::monostate, std::string, double, int64_t>;

/**
 * Metadata for a single block.
 */
struct BlockMeta {
    /**
     * Metadata for this block.
     *
     * Note that the int keys correspond to the string values in the chunk's blockMetaIdMap.
     */
    std::unordered_map<unsigned int, MetaValue> meta;
};

/**
 * Maps an 8-bit block type (as stored in the chunk slice rows) to the corresponding block UUIDs.
 * These are shared among all rows in the chunk.
 */
struct ChunkRowBlockTypeMap {
    /**
     * 8 bit ID -> block UUID array
     *
     * Note that all occurrences of the nil UUID represent free spaces in the map; these can be
     * assigned to a new UUID.
     */
    std::array<uuids::uuid, 256> idMap;
};

/**
 * Describes a single chunk, including all blocks and their metadata.
 */
struct Chunk {
    /// Block coordinate (chunk relative); these are 8 bit to save space
    using BlockCoord = std::tuple<uint8_t, uint8_t, uint8_t>;

    /// Maximum Y height of a chunk [0..kMaxY) layers are available
    constexpr static const size_t kMaxY = 256;

    /**
     * X/Z coordinates of this chunk, in world chunk coordinate space.
     */
    glm::vec2 worldPos;

    /**
     * Chunk slice pointers for each horizontal layer of the chunk. If there are no blocks at that
     * Y level, nullptr may be written.
     */
    std::array<std::shared_ptr<ChunkSlice>, kMaxY> slices;

    /**
     * Mapping of integers -> property IDs. This is used to reduce the size of block metadata that
     * has to be stored.
     */
    std::unordered_map<unsigned int, std::string> blockMetaIdMap;

    /**
     * Per-block metadata. This is indexed by an (X, Y, Z) tuple of the block position, relative to
     * this chunk.
     *
     * Note that the metadata keys can be converted to strings with `blockMetaIdMap`.
     */
    std::unordered_map<unsigned int, BlockMeta> blockMeta;

    /**
     * List of chunk slice block ID maps. These are used to map the slice row's 8-bit block IDs to
     * the UUIDs used by the rest of the engine.
     *
     * Each row indicates which one of these maps (by index) it uses.
     */
    std::vector<ChunkRowBlockTypeMap> sliceIdMaps;

    /**
     * Chunk specific metadata
     */
    std::unordered_map<std::string, MetaValue> meta;
};

}

#endif
