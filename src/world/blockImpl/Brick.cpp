#include "Brick.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include <Logging.h>

using namespace world::blocks;

Brick *Brick::gShared = nullptr;

/**
 * Registers the brick block type.
 */
void Brick::Register() {
    gShared = new Brick;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Brick::Brick() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.brick";
    this->id = uuids::uuid::from_string("F0197386-B6F8-4E3E-8591-72CF39899F0E");

    // register textures
    this->diffuse = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/brick/all.png", out);
    });
    this->material = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/brick/material_all.png", out);
    });
    this->normal = BlockRegistry::registerTexture(Type::kTypeBlockNormal,
            glm::ivec2(128, 128), [](auto &out) {
        TextureLoader::load("block/brick/normal_all.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/brick/inventory.png", out);
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->diffuse);
    BlockRegistry::appearanceSetMaterial(this->appearanceId, this->material);
    BlockRegistry::appearanceSetNormal(this->appearanceId, this->normal);
}


/**
 * Returns the default brick block appearance.
 */
uint16_t Brick::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    return this->appearanceId;
}

