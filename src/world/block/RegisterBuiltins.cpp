#include "BlockRegistry.h"

#include "world/blockImpl/Dirt.h"
#include "world/blockImpl/Stone.h"

using namespace world::blocks;

/**
 * Registers all built in blocks.
 */
void world::RegisterBuiltinBlocks() {
    Dirt::Register();
    Stone::Register();
}
