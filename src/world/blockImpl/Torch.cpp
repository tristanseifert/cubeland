#include "Torch.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include "gfx/lights/PointLight.h"
#include "particles/System.h"

#include "io/Format.h"
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


/**
 * Adds chunk observers on load such that we can determine when torches are removed.
 */
void Torch::chunkWasLoaded(std::shared_ptr<Chunk> chunk) {
    const auto &chunkPos = chunk->worldPos;

    // add change handlers
    using namespace std::placeholders;
    const auto token = chunk->registerChangeCallback(std::bind(&Torch::blockDidChange, this,
                _1, _2, _3, _4));

    std::lock_guard<std::mutex> lg(this->chunkObserversLock);
    this->chunkObservers[chunkPos] = token;
}

/**
 * Remove particle systems for all torches in the given chunk.
 */
void Torch::chunkWillUnload(std::shared_ptr<Chunk> chunk) {
    const auto &chunkPos = chunk->worldPos;

    // remove change handler
    std::lock_guard<std::mutex> lg(this->chunkObserversLock);

    if(this->chunkObservers.contains(chunkPos)) {
        chunk->unregisterChangeCallback(this->chunkObservers[chunkPos]);
        this->chunkObservers.erase(chunkPos);
    }
}

/**
 * Create torch particle systems (as needed) when torches are loaded into the world
 */
void Torch::blockWillDisplay(const glm::ivec3 &pos) {
    this->addedTorch(pos);
}

/**
 * Chunk change callback
 */
void Torch::blockDidChange(world::Chunk *chunk, const glm::ivec3 &blockCoord, const world::Chunk::ChangeHints hints, const uuids::uuid &blockId) {
    // ignore all non-torch blocks
    if(blockId != this->id) return;

    auto worldPos = blockCoord;
    worldPos += glm::ivec3(chunk->worldPos.x * 256, 0, chunk->worldPos.y * 256);

    // if a torch was added, create its particle system
    if(hints & Chunk::ChangeHints::kBlockAdded) {
        this->addedTorch(worldPos);
    }
    // a torch was removed
    else if(hints & Chunk::ChangeHints::kBlockRemoved) {
        this->removedTorch(worldPos);
    }
}



/**
 * Creates a torch's particle system when it appears, if it doesn't exist already.
 */
void Torch::addedTorch(const glm::ivec3 &worldPos) {
    std::lock_guard<std::mutex> lg(this->infoLock);

    // bail if we've already got torch info for that position
    if(this->info.contains(worldPos)) {
        return;
    }

    // create particle system
    const auto particleOrigin = glm::vec3(worldPos) + glm::vec3(.5, .74, .5);

    auto sys = std::make_shared<particles::System>(particleOrigin);
    this->addParticleSystem(sys);

    // create its light
    auto light = std::make_shared<gfx::PointLight>();
    light->setPosition(particleOrigin);
    light->setColors(kLightColor, glm::vec3(.4, .4, .4));
    light->setLinearAttenuation(kLinearAttenuation);
    light->setQuadraticAttenuation(kQuadraticAttenuation);

    this->addLight(std::dynamic_pointer_cast<gfx::lights::AbstractLight>(light));

    // build info struct and save it
    Info i;

    i.smoke = sys;
    i.light = light;

    this->info[worldPos] = i;
}

/**
 * Removes a torch's particle systems when removed.
 */
void Torch::removedTorch(const glm::ivec3 &worldPos) {
    std::lock_guard<std::mutex> lg(this->infoLock);

    // bail if we've not got torch info at that position
    if(!this->info.contains(worldPos)) {
        Logging::error("Removing torch at {} with no torch info!", worldPos);
        return;
    }

    // remove the particle system
    auto &i = this->info[worldPos];

    this->removeParticleSystem(i.smoke);
    this->removeLight(std::dynamic_pointer_cast<gfx::lights::AbstractLight>(i.light));

    // remove the info object
    this->info.erase(worldPos);
}
