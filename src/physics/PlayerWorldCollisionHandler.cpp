#include "PlayerWorldCollisionHandler.h"
#include "util/Intersect.h"
#include "render/scene/SceneRenderer.h"
#include "world/chunk/Chunk.h"
#include "world/block/BlockRegistry.h"
#include "io/Format.h"

#include <Logging.h>
#include <mutils/time/profiler.h>
#include <glm/glm.hpp>


using namespace physics;

/**
 * Sets up the player/world block collision handler.
 */
PlayerWorldCollisionHandler::PlayerWorldCollisionHandler(std::shared_ptr<render::SceneRenderer> &_scene) :
    scene(_scene) {
    // fuck me
}

/**
 * Checks collision state of the given position.
 *
 * For simplicity, we assume that we're two blocks in height.
 */
bool PlayerWorldCollisionHandler::isPositionOk(const glm::vec3 &pos) {
    PROFILE_SCOPE(CheckPlayerWorldCollision);

    // minY/maxY of the character bounding box
    const float minY = floor(pos.y), maxY = minY + 2.;
    const auto lb = glm::vec3(pos.x-.5, minY, pos.z-.5), rt = glm::vec3(pos.x+.5, maxY, pos.z+.5);

    /*
     * Check whether we intersect ANY block in a 1 block radius from us. If we intersect any block,
     * it's likely that the given position would put us inside geometry. It's not a particularly
     * exact way of doing it
     *
     * This could be optimized to ignore blocks that are behind us (perhaps by fustrum culling
     * them first) as well.
     */
    for(int xOff = -1; xOff <= 1; xOff++) {
        for(int zOff = -1; zOff <= 1; zOff++) {
            // we want to check intersection in the center block too
            // if(!xOff && !zOff) continue;

            for(int yOff = 0; yOff < 2; yOff++) {
                // build block bounding box
                const glm::vec3 blockLb = floor(pos) + glm::vec3(xOff, yOff, zOff),
                      blockRt = blockLb + glm::vec3(1.);

                // if we intersect, check if that block is solid
                if(util::Intersect::boxBox(lb, rt, blockLb, blockRt)) {
                    // get chunk pos for block
                    glm::ivec2 chunkPos;
                    glm::ivec3 blockOff;
                    world::Chunk::absoluteToRelative(blockLb, chunkPos, blockOff);

                    // sample block at this position
                    auto chunk = this->scene->getChunk(chunkPos);
                    if(!chunk) continue;

                    auto block = chunk->getBlock(blockOff);
                    if(!block) continue;

                    // check if the block is air type
                    if(world::BlockRegistry::isCollidableBlock(*block)) {
                        Logging::trace("Intersecting with solid block {} -> chunk {}, offset {}", blockLb, chunkPos, blockOff);
                        return false;
                    }

                }
            }
        }
    }

    // if we get here, we didn't intersect anything
    return true;
}

