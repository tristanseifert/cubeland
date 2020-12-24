#include "Dirt.h"

#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

using namespace world::blocks;

std::shared_ptr<Dirt> Dirt::gShared = nullptr;

/**
 * Registers the dirt block type.
 */
void Dirt::Register() {
    gShared = std::make_shared<Dirt>();
    BlockRegistry::registerBlock(gShared->getId(), std::dynamic_pointer_cast<Block>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Dirt::Dirt() {
    // set id and name
    this->internalName = "me.tseifert.cubeland.block.dirt";
    this->id = uuids::uuid::from_string("2be68612-133b-40c6-8436-189d4bd87a4e");

    // register textures
    this->normalTextures[0] = BlockRegistry::registerTexture(glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("/block/dirt/top.png", out);
    });
    this->normalTextures[1] = BlockRegistry::registerTexture(glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("/block/dirt/bottom.png", out);
    });
    this->normalTextures[2] = BlockRegistry::registerTexture(glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("/block/dirt/side.png", out);
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->normalTextures);

    this->noGrassAppearance = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->noGrassAppearance, this->normalTextures[1],
            this->normalTextures[1], this->normalTextures[1]);
}


/**
 * Returns the default dirt block appearance.
 */
uint16_t Dirt::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    // if the top is exposed, use the normal "grass" appearance
    if((flags & kExposedYPlus) == 0) {
        return this->appearanceId;
    }
    // otherwise, use the dirt only appearance
    else {
        return this->noGrassAppearance;
    }
}

