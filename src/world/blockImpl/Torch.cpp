#include "Torch.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include <Logging.h>

using namespace world::blocks;

/**
 * Model for a vertical torch. This is the same as a block, but it's only .15 units in width/depth
 * and .74 units tall.
 */
const world::BlockRegistry::Model Torch::kVerticalModel = {
    .vertices = {
        // top face
        {.35, .74, .65}, {.65, .74, .65}, {.65, .74, .35}, {.35, .74, .35},
        // left face
        {.35,   0, .65}, {.35, .74, .65}, {.35, .74, .35}, {.35,   0, .35},
        // right face
        {.65,   0, .35}, {.65, .74, .35}, {.65, .74, .65}, {.65,   0, .65},
        // front face
        {.35, .74, .35}, {.65, .74, .35}, {.65,   0, .35}, {.35,   0, .35},
        // back face
        {.35,   0, .65}, {.65,   0, .65}, {.65, .74, .65}, {.35, .74, .65},
    },
    .faceVertIds = {
        // top face
        {1, 0}, {1, 1}, {1, 2}, {1, 3},
        // left face
        {2, 0}, {2, 1}, {2, 2}, {2, 3},
        // right face
        {3, 0}, {3, 1}, {3, 2}, {3, 3},
        // front face
        {4, 0}, {4, 1}, {4, 2}, {4, 3},
        // back face
        {5, 0}, {5, 1}, {5, 2}, {5, 3},
    },
    .indices = {
        // top face
        0, 1, 2, 2, 3, 0,
        // left face
        4, 5, 6, 6, 7, 4,
        // right face
        8, 9, 10, 10, 11, 8,
        // front face
        12, 13, 14, 14, 15, 12,
        // back face
        16, 17, 18, 18, 19, 16,
    }
};

Torch *Torch::gShared = nullptr;

/**
 * Registers the torch block type.
 */
void Torch::Register() {
    gShared = new Torch;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Torch::Torch() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.torch";
    this->id = uuids::uuid::from_string("0ACDFBDF-9B26-459D-AA4A-5D09FEB25C94");

    // register textures
    this->texture = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(32, 32), [](auto &out) {
        TextureLoader::load("block/stone/all.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/stone/inventory.png", out);
    });

    // and register the model
    this->modelVertical = BlockRegistry::registerModel(kVerticalModel);

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->texture, this->texture, this->texture);

    Logging::trace("Torch appearance {}, texture {}, vertical model {}", 
            this->appearanceId, this->texture, this->modelVertical);
}


/**
 * Returns the default torch block appearance.
 */
uint16_t Torch::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    return this->appearanceId;
}

/**
 * Returns the appropriate model based on the blocks which this torch is adjacent to.
 */
uint16_t Torch::getModelId(const glm::ivec3 &pos, const BlockFlags flags) {
    // TODO: select correct model
    return this->modelVertical;
}
