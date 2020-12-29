#include "BlockInteractions.h"

#include "inventory/Manager.h"
#include "world/WorldSource.h"
#include "world/chunk/Chunk.h"
#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"
#include "world/tick/TickHandler.h"
#include "render/scene/SceneRenderer.h"
#include "render/scene/ChunkLoader.h"

#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <glm/glm.hpp>
#include <SDL.h>

#include <utility>
#include <vector>
#include <cmath>

using namespace input;

/**
 * Sets up the block interactions controller.
 */
BlockInteractions::BlockInteractions(std::shared_ptr<render::SceneRenderer> _scn, 
        std::shared_ptr<world::WorldSource> _src, inventory::Manager *_inv) : scene(_scn), 
        source(_src), inventory(_inv) {
    this->tickCb = world::TickHandler::add(std::bind(&BlockInteractions::destroyTickCallback, this));
}

/**
 * Removes tick callback.
 */
BlockInteractions::~BlockInteractions() {
    world::TickHandler::remove(this->tickCb);
}

/**
 * Handles SDL events. Specifically, we care about mouse down and mouse up messages, and will
 * absorb them always if we're enabled.
 *
 * This does the following:
 *
 * - Left click: remove the block under the cursor.
 * - Right click: Place a block on top of the current cursor location.
 */
bool BlockInteractions::handleEvent(const SDL_Event &event) {
    PROFILE_SCOPE(BlockInteractionsEvent);

    // bail if disabled
    if(!this->enabled) return false;

    // mouse down
    if(event.type == SDL_MOUSEBUTTONDOWN) {
        if(event.button.button == SDL_BUTTON_LEFT) {
            this->destroyBlock();
        }
        else if(event.button.button == SDL_BUTTON_RIGHT) {
            this->placeBlock();
        }
        // ignore other mouse buttons
        else {
            return false;
        }
        return true;
    }
    // mouse up
    else if(event.type == SDL_MOUSEBUTTONUP) {
        // cancel destroy timer if any
        if(event.button.button == SDL_BUTTON_LEFT) {
            // ensure the selection color is restored
            if(this->destroyTimerActive) {
                this->destroyTimerActive = false;
                this->updateDestroyProgress(false);
            }
        }

        return true;
    }

    // not handled if we fall down here
    return false;
}

/**
 * Replaces the block at the current position with air.
 */
void BlockInteractions::destroyBlock() {
    using namespace world;
    PROFILE_SCOPE(DestroyBlock);

    // bail if no selection
    auto sel = this->scene->getSelectedBlockPos();
    if(!sel.has_value()) return;

    auto [pos, relBlock] = *sel;

    // get the chunk for this block
    glm::ivec2 chunkPos(floor(pos.x / 256.), floor(pos.z / 256.));
    // Logging::trace("Destroy block: {} (in chunk {}) relative {}", pos, chunkPos, relBlock);

    auto chunk = this->scene->getChunk(chunkPos);
    if(!chunk) return;

    // get ID and block info
    const auto oldId = chunk->getBlock(relBlock);
    if(!oldId || oldId->is_nil()) return;

    const auto block = BlockRegistry::getBlock(*oldId);
    XASSERT(block, "Failed to get block for id {}", uuids::to_string(*oldId));

    // figure out how long the block needs to consume 
    const auto ticksToDestroy = block->destroyTicks();

    if(!ticksToDestroy) {
        // it's immediate, so don't bother with the timer
        bool collected = this->inventory->addItem(*oldId);
        chunk->setBlock(relBlock, BlockRegistry::kAirBlockId);
        this->scene->forceSelectionUpdate();
    }
    // set the timer
    else {
        this->destroyTimer = this->destroyTimerTotal = ticksToDestroy;
        this->destroyPos = *sel;
        this->destroyTimerActive = true;
    }
}

/**
 * Places a block.
 *
 * The block will be placed on the closest exposed edge of the selected block.
 */
void BlockInteractions::placeBlock() {
    PROFILE_SCOPE(PlaceBlock);

    // bail if no selection
    auto sel = this->scene->getSelectedBlockPos();
    if(!sel.has_value()) return;

    auto [selectionPos, selectionRelBlock] = *sel;

    // calculate distance to all exposed blocks (from camera pos)
    std::vector<std::pair<glm::ivec3, float>> exposed;

    const auto camPos = this->scene->getCameraPos();

    for(int i = -1; i <= 1; i += 2) {
        glm::ivec3 pos;

        // Y direction
        pos = selectionPos + glm::ivec3(0, i, 0);
        if(this->allowPlacementAt(pos)) {
            exposed.emplace_back(pos, glm::distance(camPos, glm::vec3(pos)));
        }

        // X direction
        pos = selectionPos + glm::ivec3(i, 0, 0);
        if(this->allowPlacementAt(pos)) {
            exposed.emplace_back(pos, glm::distance(camPos, glm::vec3(pos)));
        }

        // Z direction
        pos = selectionPos + glm::ivec3(0, 0, i);
        if(this->allowPlacementAt(pos)) {
            exposed.emplace_back(pos, glm::distance(camPos, glm::vec3(pos)));
        }
    }

    if(exposed.empty()) return;

    // sort and pick the closest (first) one
    std::sort(std::begin(exposed), std::end(exposed), [](const auto &l, const auto &r) {
        return (l.second < r.second);
    });

    const glm::ivec3 placeAt = exposed[0].first;
    glm::ivec2 placeAtChunk;
    glm::ivec3 placeAtRel;

    world::Chunk::absoluteToRelative(placeAt, placeAtChunk, placeAtRel);

    // get the chunk for that block position and place the block
    // Logging::trace("Selection {}, place at {} (chunk {} rel {})", selectionPos, placeAt, placeAtChunk, placeAtRel);

    auto chunk = this->scene->getChunk(placeAtChunk);
    if(!chunk) return;

    // get one of the block in the current slot
    const auto id = this->inventory->dequeueSlotBlock();
    if(id) {
        chunk->setBlock(placeAtRel, *id);

        this->scene->forceSelectionUpdate();
        this->source->markChunkDirty(chunk);
    }
}

/**
 * Checks whether the given position allows us to place a block there, e.g. whether it's air.
 */
bool BlockInteractions::allowPlacementAt(const glm::ivec3 &pos) {
    using namespace world;

    // decompose the block position
    glm::ivec2 chunkPos;
    glm::ivec3 blockPos;

    world::Chunk::absoluteToRelative(pos, chunkPos, blockPos);

    // get the chunk
    auto chunk = this->scene->getChunk(chunkPos);
    if(!chunk) return false;

    // get slice (if no slice, the entire slice is all air so allow placing)
    auto slice = chunk->slices[blockPos.y];
    if(!slice) return true;

    // get row (if no row, it's all air)
    auto row = slice->rows[blockPos.z];
    if(!row) return true;

    // sample the row and check if it's air
    const auto &map = chunk->sliceIdMaps[row->typeMap];
    const uint8_t temp = row->at(blockPos.x);
    const auto id = map.idMap[temp];

    return BlockRegistry::isAirBlock(id);
}



/**
 * Tick callback to handle destroying blocks
 */
void BlockInteractions::destroyTickCallback() {
    PROFILE_SCOPE(DestroyBlock);

    // bail if the timer isn't active
    if(!this->destroyTimerActive) {
        // reset the selection color
        if(this->destroyTimerTotal) {
            this->updateDestroyProgress();
            this->destroyTimerTotal = 0;
        }
    }

    // handle expiration
    if(--this->destroyTimer == 0) {
        world::TickHandler::defer(std::bind(&BlockInteractions::destroyBlockTimerExpired, this));

        // clear timer
        this->destroyTimerActive = false;
    }

    // update progress
    this->updateDestroyProgress();
}

/**
 * Block destruction timer expired. This is deferred to the main thread.
 */
void BlockInteractions::destroyBlockTimerExpired() {
    auto [pos, relBlock] = this->destroyPos;

    glm::ivec2 chunkPos(floor(pos.x / 256.), floor(pos.z / 256.));
    auto chunk = this->scene->getChunk(chunkPos);
    if(!chunk) return;

    // get ID and block info
    const auto oldId = chunk->getBlock(relBlock);
    if(!oldId) return;

    // Logging::trace("Collected block: {}", uuids::to_string(*oldId));

    // perform destruction of block
    bool collected = this->inventory->addItem(*oldId);
    chunk->setBlock(relBlock, world::BlockRegistry::kAirBlockId);

    this->scene->forceSelectionUpdate();
    this->source->markChunkDirty(chunk);
}

/**
 * Updates the progress indicator for block destruction.
 */
void BlockInteractions::updateDestroyProgress(bool defer) {
    float progress = 0;

    // update the color of the selection
    if(this->destroyTimerActive) {
        progress = 1. - (((float) this->destroyTimer) / ((float) this->destroyTimerTotal));
    }

    const auto color = glm::mix(kSelectionColor, kSelectionCollectedColor, progress);

    auto yeet = [&, color]{
        this->scene->setSelectionColor(color);
    };

    if(defer) {
        world::TickHandler::defer(yeet);
    } else {
        yeet();
    }
}
