/**
 * In-memory representation of a chunk, as well as per-chunk metadata.
 *
 * For simplicity, the chunk is also where per-block metadata is stored, when in memory. These
 * per block metadata use integer keys, rather than string keys; a separate map establishes the
 * mapping of chunk local integers to the global string values.
 */
#ifndef WORLD_CHUNK_CHUNK_H
#define WORLD_CHUNK_CHUNK_H

#include "ChunkSlice.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <array>
#include <map>
#include <unordered_map>
#include <string>
#include <variant>
#include <tuple>
#include <vector>
#include <list>
#include <atomic>
#include <functional>
#include <optional>
#include <mutex>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <uuid.h>

namespace render::scene {
class ChunkLoader;
}

namespace world {

struct ChunkSlice;
struct ChunkSliceTypeMap;

/// Types that may be held as chunk metadata values
using MetaValue = std::variant<std::monostate, bool, std::string, double, int64_t>;

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
    friend class render::scene::ChunkLoader;

    public:
        /**
         * Hints provided to a change callback to indicate what changed about the given block.
         */
        enum class ChangeHints: uint32_t {
            kNone                       = 0,
            kBlockRemoved               = (1 << 0),
            kBlockAdded                 = (1 << 1),
        };

        /// Block coordinate (chunk relative); these are 8 bit to save space
        /// Packed block coordinate is in the format 0x00YYZZXX.
        using BlockCoord = uint32_t;

        using ChangeToken = uint32_t;
        using ChangeCallback = std::function<void(Chunk *, const glm::ivec3 &, const ChangeHints)>;

    public:
        /// Position of the Y position in the block coordinate integer
        constexpr static const uint32_t kBlockYPos = 16;
        /// Mask for the Y component of the block coordinate
        constexpr static const uint32_t kBlockYMask = 0x00FF0000;

        /// Maximum Y height of a chunk [0..kMaxY) layers are available
        constexpr static const size_t kMaxY = 256;

    public:
        /**
         * X/Z coordinates of this chunk, in world chunk coordinate space.
         */
        glm::ivec2 worldPos;

        /**
         * Chunk slice pointers for each horizontal layer of the chunk. If there are no blocks at that
         * Y level, nullptr may be written.
         */
        std::array<ChunkSlice *, kMaxY> slices;

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
        std::map<BlockCoord, BlockMeta> blockMeta;

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

    public:
        /**
         * Releases all the memory used by slices.
         */
        ~Chunk() {
            for(auto slice : this->slices) {
                if(!slice) continue;
                delete slice;
            }
        }

    public:
        /// Gets the chunk containing an absolute world space block position
        static void absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos);
        /// Converts an absolute world space block position into a chunk and local block offset
        static void absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos, glm::ivec3 &blockPos);

    public:
        /// Adds a function to invoke any time blocks inside this chunk are changed
        ChangeToken registerChangeCallback(const ChangeCallback &callback);
        /// Removes a previously registered block change callback
        void unregisterChangeCallback(const ChangeToken token);

        /// Gets the block ID at the given chunk-relative coordinate
        std::optional<uuids::uuid> getBlock(const glm::ivec3 &pos);
        /// Sets the UUID of a block at the given chunk-relative coordinate.
        void setBlock(const glm::ivec3 &pos, const uuids::uuid &blockId, const bool prepare = false);

    private:
        /**
         * All registered chunk modification callbacks. Each of these is invoked when a block in
         * the chunk is changed.
         */
        std::unordered_map<ChangeToken, ChangeCallback> changeCbs;

        /// We need a lock to protect access to the change callbacks
        std::mutex changeCbsLock;
        /// Token for the next registration
        ChangeToken changeNextToken = 1;

    private:
        /**
         * Data is handed out from allocation blocks like this; they actually hold the memory used
         * for storing the objects.
         */
        template<typename T, size_t size> struct AllocBlock {
            // free bitmap
            // std::bitset<size> free;
            // index of the next free object
            std::atomic_uint storageUsed = 0;
            // actual objects
            T data[size];
        };

        /**
         * Information for a particular pool
         */
        template<typename T> struct Pool {
            friend class render::scene::ChunkLoader;

            /// number of elements in each storage block
            constexpr static size_t kPoolNumElements = 256;

            using Storage = AllocBlock<T, kPoolNumElements>;

        private:
            // allocated storage areas
            std::list<Storage *> storage;
            // lock protecting storage; must be used when modifying the vector
            std::mutex storageLock;

            // pointer to the next free storage block
            Storage *nextFree = nullptr;

        public:
            /**
             * Allocates one segment.
             */
            Pool() {
                this->allocSegment();
            }

            /**
             * Releases all of the allocated storage segments back to the system.
             */
            ~Pool() {
                std::lock_guard<std::mutex> lg(this->storageLock);
                for(auto storage : this->storage) {
                    delete storage;
                }
            }

            /**
             * Allocates a new object from the storage pool. If needed, this will allocate an
             * additional storage block.
             */
            T *alloc() {
                // check the free element
                if(auto ptr = this->allocFromSegment(this->nextFree)) {
                    return ptr;
                }
                // not found in the current free list. check them all

                // if we get here, we failed to allocate an object
                return nullptr;
            }

            /**
             * Releases an existing object.
             *
             * Currently, this is a no-op.
             */
            void free(T *) { /* nothing */ }

            /**
             * Returns the approximate amount of memory used by the pool
             */
            size_t estimateMemoryUse() const {
                return sizeof(Pool<T>) + (this->storage.size() * sizeof(Storage));
            }

        private:
            /**
             * Attempts to allocate an object from the given storage element. If there is no more
             * space available, nullptr is returned.
             */
            T *allocFromSegment(Storage *s) {
                // bail if full
                if(s->storageUsed == kPoolNumElements) return nullptr;
                // allocate; also increment the storage index
                auto ptr = &s->data[s->storageUsed++];

                if(s->storageUsed == kPoolNumElements) {
                    this->allocSegment();
                }

                return ptr;
            }

            /**
             * Allocates an additional segment of storage for the pool.
             */
            void allocSegment() {
                auto segment = new Storage;

                {
                    std::lock_guard<std::mutex> lg(this->storageLock);
                    this->storage.push_back(segment);
                }
                this->nextFree = segment;
            }

            /**
             * Updates the free pointer by iterating all storage segments and finding the first one
             * with at least one empty space.
             */
            void updateNextPtr() {
                std::lock_guard<std::mutex> lg(this->storageLock);
                for(auto storage : this->storage) {
                    if(storage->storageUsed < kPoolNumElements) {
                        this->nextFree = storage;
                        return;
                    }
                }
            }

            /**
             * Garbage collects the pool; all storage segments that are completely empty will be
             * deallocated, save for one.
             */
            void compact() {
                Storage *newFree = nullptr;
                std::lock_guard<std::mutex> lg(this->storageLock);

                for(auto i = this->storage.begin(); i != this->storage.end(); ++i) {
                    auto storage = *i;

                    // skip if not completely empty
                    if(storage->storageUsed) continue;

                    if(!newFree) {
                        newFree = storage;
                    } else {
                        // set free ptr to the element we found if the current one is to be removed
                        if(this->nextFree == storage) {
                            this->nextFree = newFree;
                        }

                        // then remove it
                        this->storage.erase(i--);
                    }
                }
            }
        };

    /*
     * Storage for all of the dense/sparse rows is allocated from these pools. When the chunk is
     * deallocated their storage is automagically deallocated as well.
     */
    private:
        Pool<ChunkSliceRowDense> poolDense;
        Pool<ChunkSliceRowSparse> poolSparse;

    public:
        ChunkSliceRowDense *allocRowDense() {
            return this->poolDense.alloc();
        }
        void releaseRowDense(ChunkSliceRowDense *row) {
            this->poolDense.free(row);
        }

        ChunkSliceRowSparse *allocRowSparse() {
            return this->poolSparse.alloc();
        }
        void releaseRowSparse(ChunkSliceRowSparse *row) {
            this->poolSparse.free(row);
        }

        /// Gets an estimation of the amount of memory used to allocate rows.
        size_t poolAllocSpace() const {
            return this->poolDense.estimateMemoryUse() + this->poolSparse.estimateMemoryUse();
        }
};

// proper bitset for chunk change handler hints (XXX: extend if we ever use more than 32 bits)
inline Chunk::ChangeHints operator|(Chunk::ChangeHints a, Chunk::ChangeHints b) {
    return static_cast<Chunk::ChangeHints>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline Chunk::ChangeHints operator|=(Chunk::ChangeHints &a, Chunk::ChangeHints b) {
    return (Chunk::ChangeHints &) ((uint32_t &) a |= (uint32_t) b);
}
}

#endif
