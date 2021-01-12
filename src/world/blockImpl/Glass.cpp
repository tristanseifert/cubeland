#include "Glass.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include <Logging.h>

using namespace world::blocks;

const uuids::uuid Glass::kBlockId =
    uuids::uuid::from_string("40E2F03B-F6E9-46D0-B2D3-A50250706149");

Glass *Glass::gShared = nullptr;

/**
 * Registers the stone  block type.
 */
void Glass::Register() {
    gShared = new Glass;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Glass::Glass() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.glass";
    this->id = kBlockId;

    // register textures
    this->diffuse = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/glass/all.png", out);
    });
    this->material = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/glass/material_all.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/glass/inventory.png", out);
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->diffuse);
    BlockRegistry::appearanceSetMaterial(this->appearanceId, this->material);
}

