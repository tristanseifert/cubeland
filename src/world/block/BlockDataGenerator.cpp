#include "BlockDataGenerator.h"
#include "BlockRegistry.h"

#include "io/Format.h"
#include <Logging.h>

#include <sstream>

using namespace world;

/// UV coordinates for a regular four vertex face
const std::array<glm::vec2, 4> BlockDataGenerator::kFaceUv = {
    glm::vec2(0, 1),
    glm::vec2(1, 1),
    glm::vec2(1, 0),
    glm::vec2(0, 0)
};


/**
 * Lays out the textures of all blocks into the texture atlas.
 */
void BlockDataGenerator::buildTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    // TODO: implement lol
}

/**
 * Builds the block data texture in the provided buffer.
 *
 * This texture has a row for each block type; each row, in turn, currently has 16 columns
 * assigned to it. These are laid out as follows:
 * -  0...1: Top face texture coordinates
 * -  2...3: Bottom face exture coordinates
 * -  4...5: Side faces texture coordinates
 *
 * Note that we leave the first row devoid of all data. Block IDs start at 1, with air having the
 * "unofficial" ID of 0 even though it's not actually a block.
 */
void BlockDataGenerator::generate(glm::ivec2 &size, std::vector<glm::vec4> &out) {
    // determine required space and reserve it
    size = glm::ivec2(kDataColumns, this->registry->getNumRegistered()+2);
    out.resize(size.x * size.y, glm::vec4(0));

    // fill in data for each block
    for(size_t i = 0; i < 1; i++) {
        this->writeBlockInfo(out, (i + 1), nullptr);

        std::stringstream yen;
        for(size_t x = 0; x < 16; x++) {
            yen << f("{} ", out[((i+1) * kDataColumns) + x]);
        }
        Logging::debug("Row data {}: {}", (i + 1), yen.str());
    }
}

/**
 * Writes block data for a the given block.
 */
void BlockDataGenerator::writeBlockInfo(std::vector<glm::vec4> &out, const size_t y, const std::shared_ptr<Block> &block) {
    size_t off = (y * kDataColumns);

    // write the texture coordinates. these are packed two to a 4-component vector
    for(size_t face = 0; face < 3; face++) {
        out[off + (face * 2) + 0] = glm::vec4(kFaceUv[0], kFaceUv[1]);
        out[off + (face * 2) + 1] = glm::vec4(kFaceUv[2], kFaceUv[3]);
    }
}

