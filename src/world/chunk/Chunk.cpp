#include "Chunk.h"
#include "world/block/BlockRegistry.h"

#include <glm/gtx/hash.hpp>
#include <mutils/time/profiler.h>

#include <stdexcept>

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
 */
void Chunk::setBlock(const glm::ivec3 &pos, const uuids::uuid &blockId) {
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

    // inscrete it
    row->set(pos.x, mapValue);

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
            i.second(pos, hints);
        }
    }
}

