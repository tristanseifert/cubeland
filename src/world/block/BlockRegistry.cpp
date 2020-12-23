#include "BlockRegistry.h"
#include "Block.h"
#include "BlockDataGenerator.h"

#include <Logging.h>

#include <array>

using namespace world;

/// shared block registry instance
std::shared_ptr<BlockRegistry> BlockRegistry::gShared = nullptr;

/*
 * Constant block IDs
 */
const uuids::uuid BlockRegistry::kAirBlockId = uuids::uuid::from_string("714a92e3-2984-4f0e-869e-14162d462760");

/**
 * Attempts to initialize the block registry
 */
void BlockRegistry::init() {
    XASSERT(!gShared, "Cannot re-initialize block registry");
    gShared = std::make_unique<BlockRegistry>();
}

/**
 * Sets up the block registry.
 */
BlockRegistry::BlockRegistry() {
    Logging::debug("Air block id: {}", uuids::to_string(kAirBlockId));

    this->dataGen = new BlockDataGenerator(this);
}
/**
 * Releases all resource we've allocated.
 */
BlockRegistry::~BlockRegistry() {
    delete this->dataGen;
}



/**
 * Registers a new block.
 *
 * Note that blocks are not designed to be de-registered later, unlike other things.
 */
void BlockRegistry::registerBlock(const uuids::uuid &id, std::shared_ptr<Block> block) {
    // ensure block is not duplicate or invalid
    XASSERT(block && !block->getId().is_nil(), "Invalid block");
    XASSERT(!gShared->blocks.contains(id), "Duplicate block registrations are not allowed! (offending id: {})",
            uuids::to_string(id));

    // build info struct
    BlockInfo info;
    info.block = block;

    // save it
    std::lock_guard<std::mutex> lg(gShared->blocksLock);

    gShared->blocks[block->getId()] = info;
}

/**
 * Returns a block by id.
 */
std::shared_ptr<Block> &BlockRegistry::getBlock(const uuids::uuid &id) {
    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    return gShared->blocks[id].block;
}



/**
 * Registers a block apperance.
 */
uint16_t BlockRegistry::registerBlockAppearance() {
    std::lock_guard<std::mutex> lg(gShared->appearancesLock);

    BlockAppearanceType appearance;
    uint16_t id = gShared->lastAppearanceId++;

    gShared->appearances[id] = appearance;

    return id;
}

/**
 * Removes a previously registered appearance.
 *
 * @note This call will throw if the appearance id is not registered.
 */
void BlockRegistry::removeBlockAppearance(const uint16_t id) {
    std::lock_guard<std::mutex> lg(gShared->appearancesLock);
    if(!gShared->appearances.erase(id)) {
        throw std::runtime_error("Appearance id was never registered");
    }
}

/**
 * Updates the texture indices used by an appearance.
 */
void BlockRegistry::appearanceSetTextures(const uint16_t id, const TextureId top,
        const TextureId bottom, const TextureId side) {
    std::lock_guard<std::mutex> lg(gShared->appearancesLock);

    gShared->appearances[id].texTop = top;
    gShared->appearances[id].texBottom = bottom;
    gShared->appearances[id].texSide = side;
}



/**
 * Adds a texture registration.
 */
BlockRegistry::TextureId BlockRegistry::registerTexture(const glm::ivec2 size,
        const std::function<void(std::vector<float> &)> &fillFunc) {
    // build info struct
    TextureReg info;
    info.size = size;
    info.fillFunc = fillFunc;

    // insert it while we hold the lock
    std::lock_guard<std::mutex> lg(gShared->texturesLock);

    TextureId id = gShared->lastTextureId++;
    info.id = id;

    gShared->textures[id] = info;

    // done :D
    return id;
}

/**
 * Removes the given texture registration.
 *
 * @note This call will throw if the texture id is not registered.
 */
void BlockRegistry::removeTexture(const TextureId id) {
    if(!gShared->textures.erase(id)) {
        throw std::runtime_error("Texture id was never registered");
    }
}

/**
 * Generates the texture atlas for block textures.
 */
void BlockRegistry::generateBlockTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    gShared->dataGen->buildTextureAtlas(size, out);
}

/**
 * Generates the block info texture data.
 */
void BlockRegistry::generateBlockData(glm::ivec2 &size, std::vector<glm::vec4> &out) {
    gShared->dataGen->generate(size, out);
}

