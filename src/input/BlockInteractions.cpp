#include "BlockInteractions.h"

#include "world/chunk/Chunk.h"
#include "world/block/BlockRegistry.h"

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
BlockInteractions::BlockInteractions(std::shared_ptr<render::SceneRenderer> _scn) : scene(_scn) {

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
    Logging::trace("Destroy block: {} (in chunk {}) relative {}", pos, chunkPos, relBlock);

    auto chunk = this->scene->getChunk(chunkPos);
    if(!chunk) return;

    // TODO: drop old one as item
    // TODO: update inventory

    // replace it with air
    // glm::ivec3 relBlock(pos->x % 256, pos->y % Chunk::kMaxY, pos->z % 256);

    chunk->setBlock(relBlock, BlockRegistry::kAirBlockId);

    // force some stuff to get recalculated
    this->scene->forceSelectionUpdate();
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

    this->absoluteToRelative(placeAt, placeAtChunk, placeAtRel);

    // get the chunk for that block position and place the block
    // Logging::trace("Selection {}, place at {} (chunk {} rel {})", selectionPos, placeAt, placeAtChunk, placeAtRel);

    auto chunk = this->scene->getChunk(placeAtChunk);
    if(!chunk) return;

    // TODO: pick correct item id
    const auto id = uuids::uuid::from_string("2be68612-133b-40c6-8436-189d4bd87a4e");

    chunk->setBlock(placeAtRel, id);
    this->scene->forceSelectionUpdate();
}

/**
 * Checks whether the given position allows us to place a block there, e.g. whether it's air.
 */
bool BlockInteractions::allowPlacementAt(const glm::ivec3 &pos) {
    using namespace world;

    // decompose the block position
    glm::ivec2 chunkPos;
    glm::ivec3 blockPos;

    this->absoluteToRelative(pos, chunkPos, blockPos);

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
 * Decomposes an absolute world space block position to a chunk position and a block position
 * inside that chunk.
 */
void BlockInteractions::absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos, glm::ivec3 &blockPos) {
    // get chunk pos
    chunkPos = glm::ivec2(floor(pos.x / 256.), floor(pos.z / 256.)); 

    // block pos
    int zOff = (pos.z % 256), xOff = (pos.x % 256);
    if(zOff < 0) {
        zOff = 256 - abs(zOff);
    } if(xOff < 0) {
        xOff = 256 - abs(xOff);
    }

    blockPos = glm::ivec3(xOff, pos.y, zOff);
}

