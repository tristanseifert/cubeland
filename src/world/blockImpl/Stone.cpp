#include "Stone.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include <world/block/BlockIds.h>
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
    this->id = kStoneBlockId;

    // register textures
    this->diffuse = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/stone/all.png", out);
    });
    this->material = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/stone/material_all.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/stone/inventory.png", out);
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->diffuse);
    BlockRegistry::appearanceSetMaterial(this->appearanceId, this->material);
}


/**
 * Returns the default stone block appearance.
 */
uint16_t Stone::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    return this->appearanceId;
}
