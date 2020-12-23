#include "Dirt.h"

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
        // top
        for(size_t i = 0; i < out.size()/4; i++) {
            out[(i*4)+0] = 1;
            out[(i*4)+3] = 1;
        }
    });
    this->normalTextures[1] = BlockRegistry::registerTexture(glm::ivec2(32, 32), [](auto &out) {
        // bottom
        for(size_t i = 0; i < out.size()/4; i++) {
            out[(i*4)+1] = 1;
            out[(i*4)+3] = 1;
        }
    });
    this->normalTextures[2] = BlockRegistry::registerTexture(glm::ivec2(32, 32), [](auto &out) {
        // sides
        for(size_t i = 0; i < out.size()/4; i++) {
            out[(i*4)+2] = 1;
            out[(i*4)+3] = 1;
        }
    });

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->normalTextures);
}


/**
 * Returns the default dirt block appearance.
 */
uint16_t Dirt::getBlockId(const glm::ivec3 &pos) {
    return this->appearanceId;
}

