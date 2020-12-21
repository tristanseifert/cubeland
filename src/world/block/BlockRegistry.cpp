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
 * Generates the block info texture data.
 */
void BlockRegistry::generateBlockData(glm::ivec2 &size, std::vector<glm::vec4> &out) {
    gShared->dataGen->generate(size, out);
}



/**
 * Registers a new block.
 */
void BlockRegistry::registerBlock(const uuids::uuid &id, std::shared_ptr<Block> &block) {
    // ensure block is not duplicate or invalid
    XASSERT(block && !block->getId().is_nil(), "Invalid block");
    XASSERT(!this->blocks.contains(id), "Duplicate block registrations are not allowed! (offending id: {})",
            uuids::to_string(id));

    // build info struct
    BlockInfo info;
    info.renderId = this->lastRenderId++;
    info.block = block;

    // save it
    this->blocks[block->getId()] = info;
}

