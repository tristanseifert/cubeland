#include "FileWorldReader.h"

#include "chunk/Chunk.h"
#include "chunk/ChunkSlice.h"

#include "util/LZ4.h"
#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <sqlite3.h>

#include <set>

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
    SliceState state;

    for(const auto [y, sliceId] : sliceIds) {
        this->loadSlice(state, sliceId, chunk, y);
    }

    // convert the 8 -> 16 maps we've built to 8 -> UUID
    {
        PROFILE_SCOPE(ConvertMap);

        chunk->sliceIdMaps.clear();
        for(size_t i = 0; i < state.maps.size(); i++) {
            const auto &inMap = state.maps[i];

            ChunkRowBlockTypeMap m;
            XASSERT(inMap.size() == m.idMap.size(), "mismatched id map sizes");

            for(size_t j = 0; j < inMap.size(); j++) {
                const uint16_t blockId = inMap[j];

                // copy ID directly
                if(this->blockIdMap.contains(blockId)) {
                    m.idMap[j] = this->blockIdMap[blockId];
                } else {
                    // 0xFFFF = unused entry
                    if(blockId == 0xFFFF) {
                        m.idMap[j] = uuids::uuid();
                    } else {
                        throw std::runtime_error(f("Invalid block id 0x{:4x}", blockId));
                    }
                }
            }

            chunk->sliceIdMaps.push_back(m);
        }
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
void FileWorldReader::loadSlice(SliceState &state, const int sliceId, std::shared_ptr<Chunk> chunk, const int y) {
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
            // this could indicate that there just isn't any metadata. safe to ignore
        }

        sqlite3_finalize(stmt);
    }

    // deserialize metadata into the chunk, and load grid into the temporary slice grid buffer
    this->deserializeSliceBlocks(chunk, y, gridBytes);
    this->deserializeSliceMeta(chunk, y, blockMetaBytes);

    // allocate the slice and process each row
    auto slice = std::make_shared<ChunkSlice>();

    for(size_t z = 0; z < 256; z++) {
        this->processSliceRow(state, chunk, slice, z);
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
void FileWorldReader::processSliceRow(SliceState &state, std::shared_ptr<Chunk> chunk, std::shared_ptr<ChunkSlice> slice, const size_t z) {
    PROFILE_SCOPE(ProcessRow);

    std::shared_ptr<ChunkSliceRow> row = nullptr;
    uint16_t mostFrequentBlock = 0;

    // get pointer to this row's data
    const auto ptr = this->sliceTempGrid.data() + (z * 256);

    // step 1. count unique block IDs and determine whether to use sparse/dense
    std::set<uint16_t> blockIds;
    std::multiset<uint16_t> blockIdFrequency;

    {
        PROFILE_SCOPE(AnalyzeIds);

        // get the unique block IDs, _and_ in effect build a histogram
        for(size_t x = 0; x < 256; x++) {
            blockIds.insert(ptr[x]);
            blockIdFrequency.insert(ptr[x]);
        }

        /*
         * Select a sparse representation only if there is one single block that accounts for > 75%
         * of the blocks in this row. The most frequently occurring block will be selected as the
         * sparse base.
         */
        bool useSparse = false;

        for(auto block : blockIds) {
            if(blockIdFrequency.count(block) > (256 * .75)) {
                // there can only ever be one block > 75%
                mostFrequentBlock = block;
                useSparse = true;
                break;
            }
        }

        if(useSparse) {
            row = std::make_shared<ChunkSliceRowSparse>();
        } else {
            row = std::make_shared<ChunkSliceRowDense>();
        }
    }

    // step 2. find 8 bit block ID -> UUID map, or create one
    int mapId = -1;

    {
        PROFILE_SCOPE(FindIdMap);

        /*
         * Now that we have a set of all of the 16-bit block IDs used by this row, we can use this
         * to see if we've generated a map containing these 16-bit IDs in our slice struct. This
         * avoids the need to muck about with UUIDs which is relatively slow.
         */
        for(size_t i = 0; i < state.reverseMaps.size(); i++) {
            const auto &map = state.reverseMaps[i];

            for(const auto id : blockIds) {
                if(!map.contains(id)) goto beach;
            }

            // this map contains all block IDs, yay
            mapId = i;
            // we jump down here if the map does _not_ contain one of the IDs in this row
beach:;
        }

        /*
         * Create a new mapp if there is none that fit it.
         */
        if(mapId == -1) {
            std::array<uint16_t, 256> map;
            std::fill(map.begin(), map.end(), 0xFFFF);

            std::unordered_map<uint16_t, uint8_t> reverse;

            size_t i = 0;
            for(const auto &blockId : blockIds) {
                map[i] = blockId;
                reverse[blockId] = i;
                ++i;
            }

            mapId = state.maps.size();
            state.maps.push_back(map);
            state.reverseMaps.push_back(reverse);
        }

        /**
         * We've (hopefully) selected a map by now. Store it, and if the row is dense, convert its
         * base block id value.
         */
        XASSERT(mapId >= 0, "Failed to select map id");

        row->typeMap = mapId;

        auto sparse = dynamic_pointer_cast<ChunkSliceRowSparse>(row);
        if(sparse) {
            sparse->defaultBlockId = state.reverseMaps[mapId].at(mostFrequentBlock);
        }
    }

    // step 3: fill data into the row
    {
        PROFILE_SCOPE(Fill);
        const auto &map = state.reverseMaps[mapId];

        for(size_t x = 0; x < 256; x++) {
            row->set(x, map.at(ptr[x]));
        }
    }

    // last, store the row in the slice
    slice->rows[z] = row;
}
