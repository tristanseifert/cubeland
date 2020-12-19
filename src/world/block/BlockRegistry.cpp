#include "BlockRegistry.h"

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
}
