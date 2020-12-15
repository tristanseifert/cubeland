/**
 * Memory representation of a horizontal (Y) slice of chunk data.
 *
 * Each chunk, in turn, is made up of multiple rows. Rows can be stored either as sparse or dense
 * arrays, depending on their primary content.
 *
 * Block IDs are represented as 8-bit integers. Each row can select independently which of the 
 * chunk's 8 bit ID -> block UUID dictionaries it uses. This primarily is used to reduce the memory
 * overhead.
 */
#ifndef WORLD_CHUNK_CHUNKSLICE_H
#define WORLD_CHUNK_CHUNKSLICE_H

#include <cstdint>
#include <array>
#include <unordered_map>

#include <uuid.h>

namespace world {

/**
 * Maps an 8-bit block type (as stored in the chunk slice rows) to the corresponding block UUIDs.
 * These are shared among all rows in the chunk.
 */
struct ChunkSliceTypeMap {
    /**
     * 8 bit ID -> block UUID array
     *
     * Note that all occurrences of the nil UUID represent free spaces in the map; these can be
     * assigned to a new UUID.
     */
    std::array<uuids::uuid, 256> idMap;
};

/**
 * Base class for chunk slice rows
 */
struct ChunkSliceRow {
    /// Index of the ID -> UUID to use
    uint8_t typeMap = 0;
};

/**
 * Represents a sparse row.
 *
 * These should be used if most of the row is one single type of block.
 */
struct ChunkSliceRowSparse: public ChunkSliceRow {
    /// Block ID to use for all blocks not described by the sparse map
    uint8_t defaultBlockId;

    /**
     * Mapping of X coordinate to block ID.
     *
     * The unordered map is usually implemented as a hash map, so accessing it this way should be
     * reasonably fast. We'll see how it fares with memory...
     */
    std::unordered_map<uint8_t, uint8_t> storage;
};

/**
 * Represents a dense row of data in a slice.
 *
 * Dense rows are typically used when more than 30-40% of the row has blocks in them.
 */
struct ChunkSliceRowDense: public ChunkSliceRow {
    /// Array of block IDs for all 256 X positions
    std::array<uint8_t, 256> storage;
}

/**
 * A single vertical (Y) layer of chunk data. This layer is divided into 256 rows, indexed by the Z
 * coordinate. Each row in turn contains 256 X columns.
 *
 * These slices may be made up of both dense and sparse rows, or be missing rows if they do not
 * contain any data.
 */
struct ChunkSlice {
    /**
     * Row data; points to either a chunk slice row, or null, if no data.
     */
    std::array<std::shared_ptr<ChunkSliceRow>, 256> rows;
}

}

#endif
