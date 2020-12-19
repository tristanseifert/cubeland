/**
 * The block registry stores the behaviors of all blocks, info on how to draw them, and so forth.
 * This allows additional blocks to be defined at later times.
 */
#ifndef WORLD_BLOCK_BLOCKREGISTRY_H
#define WORLD_BLOCK_BLOCKREGISTRY_H

#include <uuid.h>

#include <memory>

namespace world {
class BlockRegistry {
    public:
        // you should not call this
        BlockRegistry();

        /// Forces initialization of the block registry
        static void init();
        /// Releases the shared handle to the block registry
        static void shutdown() {
            gShared = nullptr;
        }

        /**
         * Determines whether the given block id is for an air block.
         */
        static bool isAirBlock(const uuids::uuid &id) {
            return (id == kAirBlockId);
        }

    public:
        const static uuids::uuid kAirBlockId;

    private:
        static std::shared_ptr<BlockRegistry> gShared;

};
}

#endif
