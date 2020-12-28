#include "Stone.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include <Logging.h>

using namespace world::blocks;

Stone *Stone::gShared = nullptr;

/**
 * Registers the stone  block type.
 */
void Stone::Register() {
    gShared = new Stone;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Stone::Stone() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.stone";
    this->id = uuids::uuid::from_string("27D25383-4466-405D-9DEE-1FCF4A6272CC");

    // register textures
    this->texture = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/stone/all.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/stone/inventory.png", out);
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->texture, this->texture, this->texture);

    Logging::trace("Stone appearance {}, texture {}", this->appearanceId, this->texture);
}


/**
 * Returns the default stone block appearance.
 */
uint16_t Stone::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    return this->appearanceId;
}
