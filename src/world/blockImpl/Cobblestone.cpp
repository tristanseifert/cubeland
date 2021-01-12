#include "Cobblestone.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include <Logging.h>

using namespace world::blocks;

const uuids::uuid Cobblestone::kBlockId =
    uuids::uuid::from_string("D9DB3021-4BAE-4E0A-BDF7-544BB5784F38");

Cobblestone *Cobblestone::gShared = nullptr;

/**
 * Registers the brick block type.
 */
void Cobblestone::Register() {
    gShared = new Cobblestone;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Cobblestone::Cobblestone() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.cobblestone";
    this->id = kBlockId;

    // register textures
    this->diffuse = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/cobblestone/all.png", out);
    });
    this->material = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/cobblestone/material_all.png", out);
    });
    this->normal = BlockRegistry::registerTexture(Type::kTypeBlockNormal,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/cobblestone/normal_all.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/cobblestone/inventory.png", out);
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->diffuse);
    BlockRegistry::appearanceSetMaterial(this->appearanceId, this->material);
    BlockRegistry::appearanceSetNormal(this->appearanceId, this->normal);
}


/**
 * Returns the default cobblestone block appearance.
 */
uint16_t Cobblestone::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    return this->appearanceId;
}

