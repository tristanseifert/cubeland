#include "BlockRegistry.h"

#include "world/blockImpl/Dirt.h"
#include "world/blockImpl/Stone.h"
#include "world/blockImpl/Torch.h"
#include "world/blockImpl/Glass.h"
#include "world/blockImpl/Brick.h"

using namespace world::blocks;

/**
 * Registers all built in blocks.
 */
void world::RegisterBuiltinBlocks() {
    // basic building blocks
    Dirt::Register();
    Stone::Register();
    
    // decoration
    Glass::Register();
    Brick::Register();
    
    // item-y blocks
    Torch::Register();
}
