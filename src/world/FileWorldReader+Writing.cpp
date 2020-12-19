/**
 * Implements the components of the file world reader to write world data to the file.
 */
#include "FileWorldReader.h"
#include "FileWorldSerialization.h"

#include "chunk/Chunk.h"
#include "chunk/ChunkSlice.h"
#include "block/BlockRegistry.h"

#include "util/LZ4.h"
#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <sqlite3.h>

#include <cereal/types/variant.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/portable_binary.hpp>

#include <sstream>

using namespace world;

/**
 * Writes the given chunk to the file.
 */
void FileWorldReader::writeChunk(std::shared_ptr<Chunk> chunk) {
    PROFILE_SCOPE(WriteChunk);

    bool blockMapDirty = false;
    int chunkId = -1;
    int err;
    sqlite3_stmt *stmt = nullptr;

    // chunk Y position -> chunk slice ID
    std::unordered_map<int, int> chunkSliceIds;

    // serialize chunk meta
    std::vector<char> metaBytes;
    this->serializeChunkMeta(chunk, metaBytes);

    // if we already have such a chunk, get its id
    if(this->haveChunkAt(chunk->worldPos.x, chunk->worldPos.y)) {
        {
            PROFILE_SCOPE(GetId);
            this->prepare("SELECT id FROM chunk_v1 WHERE worldX = ? AND worldZ = ?;", &stmt);
            this->bindColumn(stmt, 1, (int64_t) chunk->worldPos.x);
            this->bindColumn(stmt, 2, (int64_t) chunk->worldPos.y);

            err = sqlite3_step(stmt);
            if(err != SQLITE_ROW || !this->getColumn(stmt, 0, chunkId)) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to identify chunk: {}", err));
            }

            sqlite3_finalize(stmt);
        }

        // update its modification date
        {
            PROFILE_SCOPE(Update);
            this->prepare("UPDATE chunk_v1 SET modified = CURRENT_TIMESTAMP, metadata = ? WHERE id = ?;", &stmt);
            this->bindColumn(stmt, 1, metaBytes);
            this->bindColumn(stmt, 2, (int64_t) chunkId);
            err = sqlite3_step(stmt);

            if(err != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                throw std::runtime_error(f("Failed to update chunk timestamp: {}", err));
            }
            sqlite3_finalize(stmt);
        }
    }
    // otherwise, create a new chunk
    else {
        PROFILE_SCOPE(Create);

        this->prepare("INSERT INTO chunk_v1 (worldX, worldZ, metadata) VALUES (?, ?, ?);", &stmt);
        this->bindColumn(stmt, 1, (int64_t) chunk->worldPos.x);
        this->bindColumn(stmt, 2, (int64_t) chunk->worldPos.y);
        this->bindColumn(stmt, 3, metaBytes);

        err = sqlite3_step(stmt);
        if(err != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error(f("Failed to insert chunk: {}", err));
        }

        // get its inserted ID
        chunkId = sqlite3_last_insert_rowid(this->db);
        sqlite3_finalize(stmt);
    }

    // TODO: update block type map, if needed
    
    if(blockMapDirty) {
        this->writeBlockTypeMap();
    }

    // extract block metadata on a per slice basis
    std::array<ChunkSliceFileBlockMeta, 256> blockMetas;
    this->extractBlockMeta(chunk, blockMetas);

    // get all slices; figure out which ones to update, remove, or create new
    this->getSlicesForChunk(chunkId, chunkSliceIds);

    for(int y = 0; y < chunk->slices.size(); y++) {
        // delete existing chunk if the slice is null
        if(chunk->slices[y] == nullptr) {
            if(chunkSliceIds.contains(y)) {
                this->removeSlice(chunkSliceIds[y]);
            }
            // no data in either the world or this chunk for that Y level. nothing to do :)
        } 
        // we have chunk data...
        else {
            // ...and should update an existing slice
            if(chunkSliceIds.contains(y)) {
                this->updateSlice(chunkSliceIds[y], chunk, blockMetas[y], y);
            }
            // ...and don't have a slice for this Y level yet, so create it
            else {
                this->insertSlice(chunk, chunkId, blockMetas[y], y);
            }
        }
    }
}
/**
 * Gets all slices for the given chunk. A map of Y -> slice ID is filled.
 */
void FileWorldReader::getSlicesForChunk(const int chunkId, std::unordered_map<int, int> &slices) {
    PROFILE_SCOPE(GetChunkSliceIds);

    int err;
    sqlite3_stmt *stmt = nullptr;

    this->prepare("SELECT id, chunkId, chunkY FROM chunk_slice_v1 WHERE chunkId = ?;", &stmt);
    this->bindColumn(stmt, 1, (int64_t) chunkId);

    while((err = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t id, sliceY;

        if(!this->getColumn(stmt, 0, id) || !this->getColumn(stmt, 2, sliceY)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to get chunk slice");
        }
        if(sliceY >= Chunk::kMaxY) {
            sqlite3_finalize(stmt);
            throw std::runtime_error(f("Invalid Y ({}) for chunk slice {} on chunk {}", sliceY,
                    id, chunkId));
        }
        slices[sliceY] = id;
    }

    // clean up
    sqlite3_finalize(stmt);
}
/**
 * Serializes the chunk metadata into the compressed blob format.
 */
void FileWorldReader::serializeChunkMeta(std::shared_ptr<Chunk> chunk, std::vector<char> &data) {
    PROFILE_SCOPE(SerializeMeta);

    // serialize the meatadata into a ceral stream
    std::stringstream stream;
    
    {
        PROFILE_SCOPE(Archive);
        cereal::PortableBinaryOutputArchive arc(stream);
        arc(chunk->meta);
    }

    // then compress
    {
        PROFILE_SCOPE(LZ4Compress);
        const auto str = stream.str();
        const void *ptr = str.data();
        const size_t ptrLen = str.size();

        this->compressor->compress(ptr, ptrLen, data);
    }
}



/**
 * Removes slice with the given ID.
 */
void FileWorldReader::removeSlice(const int sliceId) {
    PROFILE_SCOPE(RemoveSlice);

    int err;
    sqlite3_stmt *stmt = nullptr;

    this->prepare("DELETE FROM chunk_slice_v1 WHERE id = ?;", &stmt);
    this->bindColumn(stmt, 1, (int64_t) sliceId);

    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to delete slice ({}): {}", err, sqlite3_errmsg(this->db)));
    }
    sqlite3_finalize(stmt);
}

/**
 * Inserts a new slice into the file.
 */
void FileWorldReader::insertSlice(std::shared_ptr<Chunk> chunk, const int chunkId, const ChunkSliceFileBlockMeta &meta, const int y) {
    PROFILE_SCOPE(InsertSlice);

    int err;
    sqlite3_stmt *stmt = nullptr;
    std::vector<char> blocks, blockMeta;

    // get the slice metadata and grid data
    this->serializeSliceBlocks(chunk, y, blocks);
    this->serializeSliceMeta(chunk, y, meta, blockMeta);

    // prepare the insertion
    PROFILE_SCOPE(Query);
    this->prepare("INSERT INTO chunk_slice_v1 (chunkId, chunkY, blocks, blockMeta) VALUES (?, ?, ?, ?)", &stmt);
    this->bindColumn(stmt, 1, (int64_t) chunkId);
    this->bindColumn(stmt, 2, (int64_t) y);
    this->bindColumn(stmt, 3, blocks);
    this->bindColumn(stmt, 4, blockMeta);

    // do it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to insert slice ({}): {}", err, sqlite3_errmsg(this->db)));
    }
    sqlite3_finalize(stmt);
}

/*
 * Updates an existing slice.
 */
void FileWorldReader::updateSlice(const int sliceId, std::shared_ptr<Chunk> chunk, const ChunkSliceFileBlockMeta &meta, const int y) {
    PROFILE_SCOPE(UpdateSlice);

    int err;
    sqlite3_stmt *stmt = nullptr;
    std::vector<char> blocks, blockMeta;

    // get the slice metadata and grid data
    this->serializeSliceBlocks(chunk, y, blocks);
    this->serializeSliceMeta(chunk, y, meta, blockMeta);

    // prepare the query
    PROFILE_SCOPE(Query);
    this->prepare("UPDATE chunk_slice_v1 SET blocks = ?, blockMeta = ?, modified = CURRENT_TIMESTAMP WHERE id = ?;", &stmt);
    this->bindColumn(stmt, 1, blocks);
    this->bindColumn(stmt, 2, blockMeta);
    this->bindColumn(stmt, 3, sliceId);

    // do it
    err = sqlite3_step(stmt);
    if(err != SQLITE_ROW && err != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(f("Failed to insert slice ({}): {}", err, sqlite3_errmsg(this->db)));
    }
    sqlite3_finalize(stmt);
}



/**
 * Encodes the block data of the slice at the specified Y level of the chunk into a 256x256 grid
 * of 16-bit values. Each 16-bit value corresponds to the block's UUID, as in the block type
 * map. The result is then compressed.
 *
 * Block metadata is serialized separately.
 */
void FileWorldReader::serializeSliceBlocks(std::shared_ptr<Chunk> chunk, const int y, std::vector<char> &data) {
    PROFILE_SCOPE(SerializeSliceBlocks);
    const auto slice = chunk->slices[y];

    // build the uuid -> block id map (invert blockIdMap)
    std::unordered_map<uuids::uuid, uint16_t> fileIdMap;
    this->buildFileIdMap(fileIdMap);

    // for each of the chunk's slice ID maps, generate an 8 bit -> file 16 bit map
    std::vector<std::array<uint16_t, 256>> chunkIdMaps;
    chunkIdMaps.reserve(chunk->sliceIdMaps.size());

    for(const auto &map : chunk->sliceIdMaps) {
        PROFILE_SCOPE(Build8To16Map);

        std::array<uint16_t, 256> ids;
        ids.fill(0xFFFF);

        XASSERT(ids.size() == map.idMap.size(), "mismatched ID map sizes");

        for(size_t i = 0; i < ids.size(); i++) {
            // ignore nil UUIDs (array is already zeroed)
            const auto uuid = map.idMap[i];
            if(uuid.is_nil()) continue;
            if(BlockRegistry::isAirBlock(uuid)) continue;

            // look it up otherwise
            ids[i] = fileIdMap.at(uuid);
        }

        chunkIdMaps.push_back(ids);
    }

    // process each row
    for(size_t z = 0; z < 256; z++) {
        PROFILE_SCOPE(ProcessRow);
        size_t gridOff = (z * 256);
        auto row = slice->rows[z];

        // skip if there's no storage for the row
        if(row == nullptr) {
            auto ptr = this->sliceTempGrid.begin() + gridOff;
            std::fill(ptr, ptr+256, 0xFFFF);
            continue;
        }

        // map directly from the slice map's 8-bit value to the file global 16 bit value
        const auto &mapping = chunkIdMaps.at(row->typeMap);

        for(size_t x = 0; x < 256; x++) {
            uint8_t temp = row->at(x);
            uint16_t value = mapping[temp];

            XASSERT(value, "Invalid value: 0x{:04x}", value);

            this->sliceTempGrid[gridOff + x] = value;
        }
    }

    // compress the grid and write it to the output buffer
    const char *bytes = reinterpret_cast<const char *>(this->sliceTempGrid.data());
    const size_t numBytes = this->sliceTempGrid.size() * sizeof(uint16_t);

    PROFILE_SCOPE(LZ4Compress);
    this->compressor->compress(bytes, numBytes, data);
}

/**
 * Builds the uuid -> file block id map. This is the inverse of the block ID map.
 */
void FileWorldReader::buildFileIdMap(std::unordered_map<uuids::uuid, uint16_t> &map) {
    PROFILE_SCOPE(BuildFileIdMap);

    for(const auto &[key, value] : this->blockIdMap) {
        map[value] = key;
    }
}



/**
 * Serializes the metadata for all blocks in a given slice.
 *
 * Since metadata is stored at the chunk granularity, this entails sifting through all block meta
 * entries and checking which match the Y level we're after.
 */
void FileWorldReader::serializeSliceMeta(std::shared_ptr<Chunk> chunk, const int y, const ChunkSliceFileBlockMeta &meta, std::vector<char> &data) {
    PROFILE_SCOPE(SerializeSliceMeta);

    // yeet it into a buffer
    std::stringstream stream;
    {
        PROFILE_SCOPE(Archive);
        cereal::PortableBinaryOutputArchive arc(stream);
        arc(meta);
    }

    // compress it pls
    {
        PROFILE_SCOPE(LZ4Compress);
        const auto str = stream.str();
        const void *ptr = str.data();
        const size_t ptrLen = str.size();

        this->compressor->compress(ptr, ptrLen, data);
    }
}



/**
 * Extracts each piece of block metadata on the given chunk by Y level. It's also converted from
 * integer to string form for saving.
 */
void FileWorldReader::extractBlockMeta(std::shared_ptr<Chunk> chunk, std::array<ChunkSliceFileBlockMeta, 256> &meta) {
    PROFILE_SCOPE(ExtractBlockMeta);

    // iterate over each of them
    for(const auto &[pos, blockMeta] : chunk->blockMeta) {
        PROFILE_SCOPE(Block);

        // get the Y pos and the corresponding slice struct
        const uint32_t y = (pos & Chunk::kBlockYMask) >> Chunk::kBlockYPos;
        auto &slice = meta[y];

        // iterate over each of the metadata keys and squelch it into this temporary struct
        std::unordered_map<std::string, ChunkSliceFileBlockMeta::ValueType> temp;
        temp.reserve(blockMeta.meta.size());

        for(const auto &[key, value] : blockMeta.meta) {
            const auto stringKey = chunk->blockMetaIdMap.at(key);
            temp[stringKey] = value;
        }

        // insert it into the appropriate slice struct
        const uint16_t coord = static_cast<uint16_t>((pos & 0x00FFFF)); // ok; block coord is YYZZXX
        slice.properties[coord] = temp;
    }
}

