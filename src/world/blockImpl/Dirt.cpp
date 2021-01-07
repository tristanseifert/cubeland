#include "Dirt.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include "io/Format.h"
#include <Logging.h>

using namespace world::blocks;

Dirt *Dirt::gShared = nullptr;

/**
 * Registers the dirt block type.
 */
void Dirt::Register() {
    gShared = new Dirt;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Dirt::Dirt() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.dirt";
    this->id = uuids::uuid::from_string("2be68612-133b-40c6-8436-189d4bd87a4e");

    // register textures (diffuse)
    this->diffuseTextures[0] = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/dirt/top.png", out);
    });
    this->diffuseTextures[1] = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/dirt/bottom.png", out);
    });
    this->diffuseTextures[2] = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/dirt/side.png", out);
    });

    // register textures (mateiral properties)
    this->materialTextures[0] = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/dirt/material_top.png", out, 4);
    });
    this->materialTextures[1] = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/dirt/material_bottom.png", out, 4);
    });
    this->materialTextures[2] = BlockRegistry::registerTexture(Type::kTypeBlockMaterial,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/dirt/material_side.png", out, 4);
    });

    // register textures (inventory)
    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/dirt/inventory.png", out);
    }); 

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->diffuseTextures);
    BlockRegistry::appearanceSetMaterial(this->appearanceId, this->materialTextures);

    this->noGrassAppearance = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->noGrassAppearance, this->diffuseTextures[1],
            this->diffuseTextures[1], this->diffuseTextures[1]);
    BlockRegistry::appearanceSetMaterial(this->noGrassAppearance, this->materialTextures[1]);
}


/**
 * Returns the default dirt block appearance.
 */
uint16_t Dirt::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    // if the top is exposed, use the normal "grass" appearance
    if((flags & kExposedYPlus)) {
        return this->appearanceId;
    }
    // otherwise, use the dirt only appearance
    else {
        return this->noGrassAppearance;
    }
}

/**
 * We want to get chunk load notifications
 */
const bool Dirt::wantsChunkLoadNotifications() const {
    return true;
}

void Dirt::chunkWasLoaded(std::shared_ptr<Chunk> chunk) {
//     Logging::trace("Loaded: {}", chunk->worldPos);
}

void Dirt::chunkWillUnload(std::shared_ptr<Chunk> chunk) {
//     Logging::trace("Unloaded: {}", chunk->worldPos);
}

