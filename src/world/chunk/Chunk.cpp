#include "Chunk.h"
#include "world/block/BlockRegistry.h"

#include <glm/gtx/hash.hpp>
#include <mutils/time/profiler.h>

#include <stdexcept>

#include "io/Format.h"
#include <Logging.h>

using namespace world;

/**
 * Registers a new block change handler. A token is returned which can later be used to deregister
 * the callback.
 */
Chunk::ChangeToken Chunk::registerChangeCallback(const ChangeCallback &callback) {
    LOCK_GUARD(this->changeCbsLock, RegisterChangeCb);

    const auto token = this->changeNextToken++;
    this->changeCbs[token] = callback;

    return token;
}

/**
 * Deregisters an existing change callback. If the callback does not exist, an exception is thrown.
 */
void Chunk::unregisterChangeCallback(const ChangeToken token) {
    LOCK_GUARD(this->changeCbsLock, UnregisterChangeCb);

    if(!this->changeCbs.erase(token)) {
        throw std::runtime_error("Invalid chunk change token");
    }
}


/**
 * Gets the value of a block at the given chunk-relative coordinate.
 */
std::optional<uuids::uuid> Chunk::getBlock(const glm::ivec3 &pos) {
    // get slice and row
    ChunkSlice *slice = this->slices[pos.y];
    if(!slice) {
        return std::nullopt;
    }
    ChunkSliceRow *row = slice->rows[pos.z];
    if(!row) {
        return std::nullopt;
    }

    // read the temp value and convert
    auto &map = this->sliceIdMaps[row->typeMap];
    const auto temp = row->at(pos.x);

    return map.idMap[temp];
}


/**
 * Sets the block at the given position to the provided UUID.
 *
 * First, this will identify the correct 8-bit mapping ID to use for the block id. There's a few
 * cases: first, the ID can be present in the current map, in which no work is done. If the current
 * map does NOT contain the ID, we search for a map that contains all IDs plus the block ID, and if
 * found, renumber all existing blocks. Otherwise, we try to insert the ID at the end of the
 * current map. If that fails due to insufficient space, we create a new map containing the subset
 * of used IDs plus the new ID, and renumber the row.
 *
 * Once the 8-bit ID value has been retrieved, we either insert the block data into the existing
 * row if it has space, or (in the case of sparse rows) expand them to have more space, possibly
 * turning them into dense allocations.
 *
 * Lastly, all block change callbacks are invoked.
 *
 * If the `prepare` argument is set, the row's prepare handler is invoked to make the row data
 * usable for iteration.
 */
void Chunk::setBlock(const glm::ivec3 &pos, const uuids::uuid &blockId, const bool prepare) {
    // get slice, or allocate if needed
    ChunkSlice *slice = this->slices[pos.y];
    if(!slice) {
        slice = new ChunkSlice;
        this->slices[pos.y] = slice;
    }

    // get row or allocate
    bool newRow = false;
    ChunkSliceRow *row = slice->rows[pos.z];
    if(!row) {
        row = new ChunkSliceRowDense;
        newRow = true;
        slice->rows[pos.z] = row;
    }

    // find the corresponding 8-bit value. may require creating maps
    // that is not implemented; this wil also blow up when we get more than 256 block types
    bool mapValueFound = false, airValueFound = false;
    uint8_t mapValue, mapAirValue;

    auto &map = this->sliceIdMaps[row->typeMap];
    for(size_t i = 0; i < map.idMap.size(); i++) {
        if(map.idMap[i] == blockId) {
            mapValue = i;
            mapValueFound = true;
            goto beach;
        }
    }

    // if the map doesn't contain the value, find the first empty slot and insert it
    if(!mapValueFound) {
        for(size_t i = 0; i < map.idMap.size(); i++) {
            if(map.idMap[i].is_nil()) {
                map.idMap[i] = blockId;
                mapValue = i;
                mapValueFound = true;
                goto beach;
            }
        }
    }

    XASSERT(mapValueFound, "Failed to find block ID map");

beach:;
    // get block type for air
    for(size_t i = 0; i < map.idMap.size(); i++) {
        if(BlockRegistry::isAirBlock(map.idMap[i])) {
            mapAirValue = i;
            airValueFound = true;
            goto dispensary;
        }
    }

    XASSERT(airValueFound, "Failed to get ID for air block");

dispensary:;
    // fill with the type for air
    if(newRow) {
       for(size_t x = 0; x < 256; x++) {
           row->set(x, mapAirValue);
       }
    }

    // if no space remaining, allocate a dense map
    if(!row->hasSpaceAvailable()) {
        // handle the source being a sparse row
        auto sparse = dynamic_cast<ChunkSliceRowSparse *>(row);
        if(sparse) {
            auto newRow = new ChunkSliceRowDense;
            newRow->typeMap = row->typeMap;

            for(size_t x = 0; x < 256; x++) {
                newRow->set(x, sparse->at(x));
            }

            // release the old row and swap it for the new
            this->releaseRowSparse(sparse);

            row = newRow;
            slice->rows[pos.z] = row;
        } else {
            XASSERT(false, "Full row, but it is not sparse! Something is fucked (row type {})",
                    typeid(row).name());
        }

    }

    // insert value. this should never fail
    row->set(pos.x, mapValue);

    if(prepare) {
        row->prepare();
    }

    // Logging::trace("Set {} (row {}) to {} (id {})", pos, (void *) row, mapValue, uuids::to_string(blockId));

    // callbacks
    ChangeHints hints = ChangeHints::kNone;

    if(BlockRegistry::isAirBlock(blockId)) {
        hints |= ChangeHints::kBlockRemoved;
    } else {
        hints |= ChangeHints::kBlockAdded;
    }

    {
        LOCK_GUARD(this->changeCbsLock, InvokeChangeCb);
        for(auto &i : this->changeCbs) {
            i.second(this, pos, hints);
        }
    }
}

/**
 * Gets the chunk that contains a particular block.
 */
void Chunk::absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos) {
    chunkPos = glm::ivec2(floor(pos.x / 256.), floor(pos.z / 256.)); 
}

/**
 * Decomposes an absolute world space block position to a chunk position and a block position
 * inside that chunk.
 */
void Chunk::absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos, glm::ivec3 &blockPos) {
    absoluteToRelative(pos, chunkPos);

    // block pos
    int zOff = (pos.z % 256), xOff = (pos.x % 256);
    if(zOff < 0) {
        zOff = 256 - abs(zOff);
    } if(xOff < 0) {
        xOff = 256 - abs(xOff);
    }

    blockPos = glm::ivec3(xOff, pos.y % 256, zOff);
}
