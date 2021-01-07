#include "BlockRegistry.h"
#include "Block.h"
#include "BlockDataGenerator.h"

#include "util/ThreadPool.h"

#include <Logging.h>

#include <array>
#include <future>
#include <vector>

using namespace world;

/// shared thread pool
using WorkItem = std::function<void(void)>;
static util::ThreadPool<WorkItem> gBlockCallbackQueue("Block Callbacks", 4);

/// shared block registry instance
BlockRegistry *BlockRegistry::gShared = nullptr;

/*
 * Constant block IDs
 */
const uuids::uuid BlockRegistry::kAirBlockId = uuids::uuid::from_string("714a92e3-2984-4f0e-869e-14162d462760");

/**
 * Attempts to initialize the block registry
 */
void BlockRegistry::init() {
    XASSERT(!gShared, "Cannot re-initialize block registry");
    gShared = new BlockRegistry;
}

/**
 * Sets up the block registry.
 */
BlockRegistry::BlockRegistry() {
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
void BlockRegistry::registerBlock(const uuids::uuid &id, Block *block) {
    // ensure block is not duplicate or invalid
    XASSERT(block && !block->getId().is_nil(), "Invalid block");
    XASSERT(!gShared->blocks.contains(id), "Duplicate block registrations are not allowed! (offending id: {})",
            uuids::to_string(id));

    const auto blockId = block->getId();

    // build info struct
    BlockInfo info;
    info.block = block;

    // save it
    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    gShared->blocks[blockId] = info;
}

/**
 * Returns a block by id.
 */
Block *BlockRegistry::getBlock(const uuids::uuid &id) {
    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    return gShared->blocks[id].block;
}

/**
 * Iterates over all blocks.
 */
void BlockRegistry::iterateBlocks(const std::function<void(const uuids::uuid &, Block *)> &cb) {
    for(const auto &[uuid, info] : gShared->blocks) {
        if(!info.block) continue;
        cb(uuid, info.block);
    }
}

/**
 * Checks if the block with the given ID can be collided with.
 */
bool BlockRegistry::isCollidableBlock(const uuids::uuid &id, const glm::ivec3 &pos) {
    if(isAirBlock(id)) return false;

    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    auto block = gShared->blocks[id].block;
    if(block) {
        return block->isCollidable(pos);
    } else {
        return false;
    }
}

/**
 * Checks whether the given block is opaque.
 */
bool BlockRegistry::isOpaqueBlock(const uuids::uuid &id) {
    if(isAirBlock(id)) return false;

    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    auto block = gShared->blocks[id].block;
    if(block) {
        return block->isOpaque();
    } else {
        return true;
    }
}

/**
 * Checks if the block with the given ID can be selected.
 */
bool BlockRegistry::isSelectable(const uuids::uuid &id, const glm::ivec3 &pos) {
    if(isAirBlock(id)) return false;

    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    auto block = gShared->blocks[id].block;
    if(block) {
        return block->isSelectable(pos);
    } else {
        return false;
    }
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
 * Updates the texture indices used by an appearance for its material properties texture.
 */
void BlockRegistry::appearanceSetMaterial(const uint16_t id, const TextureId top,
        const TextureId bottom, const TextureId side) {
    std::lock_guard<std::mutex> lg(gShared->appearancesLock);

    gShared->appearances[id].matTop = top;
    gShared->appearances[id].matBottom = bottom;
    gShared->appearances[id].matSide = side;
}


/**
 * Adds a texture registration.
 */
BlockRegistry::TextureId BlockRegistry::registerTexture(const TextureType type,
        const glm::ivec2 size, const std::function<void(std::vector<float> &)> &fillFunc) {
    // build info struct
    TextureReg info;
    info.size = size;
    info.type = type;
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
    gShared->dataGen->buildBlockTextureAtlas(size, out);
}

/**
 * Generates the block material atlas. This is a two component texture.
 */
void BlockRegistry::generateBlockMaterialTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    gShared->dataGen->buildBlockMaterialTextureAtlas(size, out);
}

/**
 * Generates the texture atlas for inventory images.
 */
void BlockRegistry::generateInventoryTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    gShared->dataGen->buildInventoryTextureAtlas(size, out);
}

/**
 * Generates the block info texture data.
 */
void BlockRegistry::generateBlockData(glm::ivec2 &size, std::vector<glm::vec4> &out) {
    gShared->dataGen->generate(size, out);
}

/**
 * Returns UV coordinates for the given texture.
 */
glm::vec4 BlockRegistry::getTextureUv(const TextureId id) {
    // ensure texture exists
    std::lock_guard<std::mutex> lg(gShared->texturesLock);
    if(!gShared->textures.contains(id)) {
        throw std::runtime_error("Invalid texture id");
    }

    switch(gShared->textures[id].type) {
        case TextureType::kTypeBlockFace:
            return gShared->dataGen->uvBoundsForBlockTexture(id);
        case TextureType::kTypeInventory:
            return gShared->dataGen->uvBoundsForInventoryTexture(id);

        // shouldn't get here
        default:
            throw std::runtime_error("Failed to get UV coords for texture");
    }
}



/**
 * Registers a new model.
 */
uint16_t BlockRegistry::registerModel(const Model &mod) {
    std::lock_guard<std::mutex> lg(gShared->modelsLock);
    const auto id = gShared->lastModelId++;

    gShared->models[id] = mod;

    return id;
}



/**
 * Invokes all registered block's "chunk loaded" handlers.
 */
void BlockRegistry::notifyChunkLoaded(std::shared_ptr<Chunk> &ptr) {
    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    std::vector<std::future<void>> futures;

    for(auto &[id, info] : gShared->blocks) {
        if(info.block && info.block->wantsChunkLoadNotifications()) {
            Block *block = info.block;
            futures.push_back(gBlockCallbackQueue.queueWorkItem([=] {
                block->chunkWasLoaded(ptr);
            }));
        }
    }

    // wait for all callbacks to complete
    for(auto &future : futures) {
        future.get();
    }}

/**
 * Notifies all blocks that a chunk was unloaded.
 *
 * Callbacks are invoked on a background queue, but serialized so that they are all complete once
 * this method returns.
 */
void BlockRegistry::notifyChunkWillUnload(std::shared_ptr<Chunk> &ptr) {
    std::lock_guard<std::mutex> lg(gShared->blocksLock);
    std::vector<std::future<void>> futures;

    for(auto &[id, info] : gShared->blocks) {
        if(info.block && info.block->wantsChunkLoadNotifications()) {
            Block *block = info.block;
            futures.push_back(gBlockCallbackQueue.queueWorkItem([=] {
                block->chunkWillUnload(ptr);
            }));
        }
    }

    // wait for all callbacks to complete
    for(auto &future : futures) {
        future.get();
    }
}

