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

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include <uuid.h>

namespace world {

/**
 * Base class for chunk slice rows
 */
struct ChunkSliceRow {
    /// Index of the ID -> UUID to use
    uint8_t typeMap = 0;

    /// return the ID value at the given index
    virtual uint8_t at(const int i) = 0;
    virtual void set(const int i, const uint8_t value) = 0;
    virtual bool containsType(const uint8_t value) = 0;
    /// whether this row has space for additional data
    virtual bool hasSpaceAvailable() const = 0;
    /// performs any internal housekeeping to prepare the row for rendering
    virtual void prepare() {};

    virtual ~ChunkSliceRow() = default;
};

/**
 * Represents a sparse row.
 *
 * These should be used if most of the row is one single type of block. The maximum number of block
 * alternates this can store is 64.
 *
 * TODO: investigate why this is broken. Either when storing or reading data, something breaks
 * pretty badly resulting in corrupted blocks.
 */
struct ChunkSliceRowSparse: public ChunkSliceRow {
    friend struct Chunk;

    /// maximum storage space available in the sparse row
    constexpr static const size_t kMaxEntries = 64;

    ChunkSliceRowSparse() {
        /// fills the storage with all F's so it compares at the end of lists when sorting
        std::fill(std::begin(this->storage), std::end(this->storage), 0xFFFF);
    }

    /// Block ID to use for all blocks not described by the sparse map
    uint8_t defaultBlockId;
    /// current amount of slots used in the sparse storage array
    uint8_t slotsUsed = 0;

    bool hasSpaceAvailable() const {
        return (this->slotsUsed < kMaxEntries);
    }

    /**
     * Mapping of X coordinate to block ID.
     *
     * This array is sorted by X position. Values are encoded as 0xPPVV, where P is the X
     * coordinate of the block, and V is the actual block ID.
     */
    std::array<uint16_t, kMaxEntries> storage;

    /// return the ID value at the given index from the sparse storage, or the default id
    virtual uint8_t at(const int i) {
        // bail early if no entries in storage
        if(!this->slotsUsed) {
            return this->defaultBlockId;
        }

        // quickly search the storage array
        uint16_t key = (i & 0xFF) << 8;
        void *ptr = ::bsearch(&key, this->storage.data(), this->slotsUsed, sizeof(uint16_t),
                &ChunkSliceRowSparse::compare);

        if(ptr) {
            return (*reinterpret_cast<const uint16_t *>(ptr)) & 0x00FF;
        }

        // not found
        return this->defaultBlockId;
    }
    /// if not the same as the default block id, inserts the given value into the sparse storage
    virtual void set(const int i, const uint8_t value) {
        const uint16_t tag = ((i & 0xFF) << 8) | value;

        // if we've already got an entry for this X position, overwrite it
        if(this->slotsUsed) {
            for(size_t j = 0; j < this->slotsUsed; j++) {
                if((this->storage[j] & 0xFF00) >> 8 == (i & 0xFF)) {
                    // overwrite if NOT the default block
                    if(value != this->defaultBlockId) {
                        this->storage[j] = tag;
                    } 
                    // otherwise, remove this entry and resize array
                    else {
                        std::remove(std::begin(this->storage),
                                std::begin(this->storage) + this->slotsUsed, this->storage[j]);
                        this->slotsUsed--;
                    }
                    return;
                }
            }
        }

        // do not write the default block id, and bail if we've no space to add an entry
        if(value == this->defaultBlockId) return;
        if(this->slotsUsed == storage.size()) {
            throw std::runtime_error("Row is full");
        }

        // otherwise, add a new one
        this->storage[this->slotsUsed++] = tag;
    }
    virtual bool containsType(const uint8_t type) {
        // check if that's the default id
        if(this->defaultBlockId == type) return true;
        // iterate through the sparse storage
        if(!this->slotsUsed) return false;
        return std::find_if(std::begin(this->storage), std::begin(this->storage) + this->slotsUsed, 
                [type](const uint16_t value){
                return (value & 0x00FF) == type;
            }) != std::end(this->storage);
    }

    // ensures the storage is ready for display
    virtual void prepare() {
        if(!this->slotsUsed) return;
        std::sort(std::begin(this->storage), std::begin(this->storage) + this->slotsUsed);
    }

    private:
    // element comparison functions; strips the lower value and takes just the high 8 bits
    static int compare(const void *_key, const void *_b) {
        uint16_t key = *((const uint16_t *) _key) & 0xFF00;
        uint16_t b = *((const uint16_t *) _b) & 0xFF00;
        return ((int) key) - ((int) b);
    }
};

/**
 * Represents a dense row of data in a slice.
 *
 * Dense rows are typically used when more than 30-40% of the row has blocks in them.
 */
struct ChunkSliceRowDense: public ChunkSliceRow {
    /// Array of block IDs for all 256 X positions
    std::array<uint8_t, 256> storage;

    /// return the ID value at the given index directly from storage
    virtual uint8_t at(const int i) {
        return this->storage[i];
    }
    virtual void set(const int i, const uint8_t value) {
        this->storage[i] = value;
    }

    virtual bool containsType(const uint8_t type) {
        return std::find(std::begin(this->storage), std::end(this->storage), type) 
            != std::end(this->storage);
    }

    // dense map can never run out of space :D
    bool hasSpaceAvailable() const {
        return true;
    }
};

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
    std::array<ChunkSliceRow *, 256> rows;

    /**
     * Lock to protect this slice to ensure only one client modifies it at a time
     */
    std::mutex mutex;

    /**
     * Ensure the chunk slice is initialized to a null state.
     */
    ChunkSlice() {
        std::fill(std::begin(this->rows), std::end(this->rows), nullptr);
    }

    void lock() {
        this->mutex.lock();
    }
    void unlock() {
        this->mutex.unlock();
    }
};

}

#endif
