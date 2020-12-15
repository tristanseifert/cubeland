#include "FileWorldReader.h"

#include "chunk/Chunk.h"
#include "chunk/ChunkSlice.h"

#include "util/LZ4.h"
#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <sqlite3.h>

using namespace world;

/**
 * Loads a chunk that exists at the given (x,z) coordinate. 
 */
std::shared_ptr<Chunk> FileWorldReader::loadChunk(int x, int z) {
    PROFILE_SCOPE(LoadChunk);

    int chunkId = -1;
    std::vector<char> metaBytes;

    int err;
    sqlite3_stmt *stmt = nullptr;

    // get chunk metadata (also serves to check if it exists)
    {
        PROFILE_SCOPE(GetId);

        this->prepare("SELECT id,metadata FROM chunk_v1 WHERE worldX = ? AND worldZ = ?;", &stmt);
        this->bindColumn(stmt, 1, (int64_t) x);
        this->bindColumn(stmt, 2, (int64_t) z);

        err = sqlite3_step(stmt);
        if(err != SQLITE_ROW || !this->getColumn(stmt, 0, chunkId)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error(f("Failed to get chunk: {}", err));
        }
        if(!this->getColumn(stmt, 1, metaBytes)) {
            Logging::warn("Failed to get metadata column (there may not be any!)");
        }

        sqlite3_finalize(stmt);
    }

    // get the IDs of all slices associated with this chunk (as a Y -> slice ID map)
    std::unordered_map<int, int> sliceIds;
    this->getSlicesForChunk(chunkId, sliceIds);

    // prepare a chunk to hold all this data and deserialize data into it
    auto chunk = std::make_shared<Chunk>();
    chunk->worldPos = glm::vec2(x, z);

    this->deserializeChunkMeta(chunk, metaBytes);

    // next, process each slice
    for(const auto [y, sliceId] : sliceIds) {
        this->loadSlice(sliceId, chunk, y);
    }

    // we're done
    return chunk;
}



/**
 * Deserializes the compressed chunk metadata.
 */
void FileWorldReader::deserializeChunkMeta(std::shared_ptr<Chunk> chunk, const std::vector<char> &compressed) {
    PROFILE_SCOPE(DeserializeMeta);

    // first, decompress it; bail if we got 0 bytes compressed text
    std::vector<char> bytes;
    this->compressor->decompress(compressed, bytes);

    if(bytes.empty()) {
        chunk->meta.clear();
        return;
    }

    // TODO: deserialize
}



/**
 * Loads a slice of data from the world file. This will:
 *
 * - Deserialize the 256x256 block grid
 * - Load metadata for all blocks in this slice
 * - Allocate a new slice to hold the data
 * - Determine whether to use a sparse/dense representation for each row
 * - Optionally create new 8 bit block ID -> UUID maps, if existing ones don't work for that row
 * - Load row data
 *
 */
void FileWorldReader::loadSlice(const int sliceId, std::shared_ptr<Chunk> chunk, const int y) {
    PROFILE_SCOPE(LoadSlice)

    std::vector<char> gridBytes;
    std::vector<char> blockMetaBytes;

    // read chunk info
    {
        PROFILE_SCOPE(Query)
        int err;
        sqlite3_stmt *stmt = nullptr;

        this->prepare("SELECT id,blocks,blockMeta FROM chunk_slice_v1 WHERE id = ?;", &stmt);
        this->bindColumn(stmt, 1, (int64_t) sliceId);

        err = sqlite3_step(stmt);

        // it is mandatory we read the blocks data; we may not have block meta though.
        if(err != SQLITE_ROW || !this->getColumn(stmt, 1, gridBytes)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error(f("Failed to get chunk slice: {}", err));
        }
        if(!this->getColumn(stmt, 2, blockMetaBytes)) {
            // Logging::warn("Failed to load block metadata for slice id {}", sliceId);
        }

        sqlite3_finalize(stmt);
    }

    // deserialize metadata into the chunk, and load grid into the temporary slice grid buffer
    this->deserializeSliceBlocks(chunk, y, gridBytes);
    this->deserializeSliceMeta(chunk, y, blockMetaBytes);

    // allocate the slice and process each row
    auto slice = std::make_shared<ChunkSlice>();

    for(size_t z = 0; z < 256; z++) {
        this->processSliceRow(chunk, slice, z);
    }

    // we're done, assign the slice to the chunk
    chunk->slices[y] = slice;
}

/**
 * Deserializes the slice block data. What this really does is just perform the decompression of
 * the blob data.
 */
void FileWorldReader::deserializeSliceBlocks(std::shared_ptr<Chunk> chunk, const int y, const std::vector<char> &compressed) {
    PROFILE_SCOPE(DeserializeSliceBlocks);

    // perform the decompression
    char *bytes = reinterpret_cast<char *>(this->sliceTempGrid.data());
    const size_t numBytes = this->sliceTempGrid.size() * sizeof(uint16_t);

    this->sliceTempGrid[0] = 0xFF;

    PROFILE_SCOPE(LZ4Decompress);
    this->compressor->decompress(compressed, bytes, numBytes);

    Logging::trace("Decompressed: {} {} {} {}", this->sliceTempGrid[0], this->sliceTempGrid[1],
            this->sliceTempGrid[2], this->sliceTempGrid[3]);
}

/**
 * Decompresses and decodes the provided raw metadata stored in the chunk slice row. This data is
 * then immediately inserted into the chunk's block metadata storage.
 */
void FileWorldReader::deserializeSliceMeta(std::shared_ptr<Chunk> chunk, const int y, const std::vector<char> &compressed) {
    PROFILE_SCOPE(DeserializeSliceMeta);

    // perform decompression and bail if no data results
    std::vector<char> bytes;
    this->compressor->decompress(compressed, bytes);

    if(bytes.empty()) {
        return;
    }

    // TODO: implement
}


/**
 * This encapsulates the block data loading steps. It's done in two passes over the row's data:
 *
 * - First, get all the unique block IDs used in the row. Check if an existing ChunkRowBlockTypeMap
 *   contains _all_ of these IDs. If not, create one with just those. Otherwise, use the existing
 *   map.
 * - Using the histogram data generated, determine whether we should use a sparse or dense row
 *   representation for this row.
 * - Using the previosuly selected map, again go over each block and fill it into the chunk slice's
 *   row data.
 */
void FileWorldReader::processSliceRow(std::shared_ptr<Chunk> chunk, std::shared_ptr<ChunkSlice> slice, const size_t z) {
    PROFILE_SCOPE(ProcessRow);

}
