#include "BlockDataGenerator.h"
#include "BlockRegistry.h"

#include "io/Format.h"
#include <Logging.h>

#include <cstring>
#include <algorithm>
#include <utility>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <mutex>

using namespace world;

// uncomment to output logging when assembling texture atlas
// #define LOG_ATLAS_LAYOUT

/// UV coordinates for a regular four vertex face
const std::array<glm::vec2, 4> BlockDataGenerator::kFaceUv = {
    glm::vec2(0, 1),
    glm::vec2(1, 1),
    glm::vec2(1, 0),
    glm::vec2(0, 0)
};


/**
 * Forces the atlas to be repacked.
 */
void BlockDataGenerator::repackBlockAtlas() {
    std::lock_guard<std::mutex> lg(this->registry->texturesLock);

    std::unordered_map<BlockRegistry::TextureId, glm::ivec2> sizes;
    sizes.reserve(this->registry->textures.size());
    for(const auto &[textureId, info] : this->registry->textures) {
        sizes[textureId] = info.size;
    }
    this->blockAtlas.updateLayout(sizes);
}

/**
 * Lays out the textures of all blocks into the texture atlas.
 */
void BlockDataGenerator::buildBlockTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    // get texture lock
    std::lock_guard<std::mutex> lg(this->registry->texturesLock);

    // build atlas layout if needed
    if(this->forceBlockAtlasUpdate) {
        Logging::debug("Rebuilding block texture atlas...");

        std::unordered_map<BlockRegistry::TextureId, glm::ivec2> sizes;
        sizes.reserve(this->registry->textures.size());

        for(const auto &[textureId, info] : this->registry->textures) {
            if(info.type != BlockRegistry::TextureType::kTypeBlockFace) continue;
            sizes[textureId] = info.size;
        }
        XASSERT(!sizes.empty(), "No textures for block face atlas!");

        this->blockAtlas.updateLayout(sizes);
        this->forceBlockAtlasUpdate = false;
    }

    this->copyAtlas(this->blockAtlas, size, out);
}



/**
 * Builds the inventory item texture atlas.
 */
void BlockDataGenerator::buildInventoryTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    // get texture lock
    std::lock_guard<std::mutex> lg(this->registry->texturesLock);

    // build atlas layout if needed
    if(this->forceInventoryAtlasUpdate) {
        Logging::debug("Rebuilding inventory texture atlas...");

        std::unordered_map<BlockRegistry::TextureId, glm::ivec2> sizes;
        sizes.reserve(this->registry->textures.size());

        for(const auto &[textureId, info] : this->registry->textures) {
            if(info.type != BlockRegistry::TextureType::kTypeInventory) continue;
            sizes[textureId] = info.size;
        }
        XASSERT(!sizes.empty(), "No textures for inventory atlas!");

        this->inventoryAtlas.updateLayout(sizes);
        this->forceInventoryAtlasUpdate = false;
    }

    // copy pixel data
    this->copyAtlas(this->inventoryAtlas, size, out);
}


/**
 * Copies pixel data out of the atlas into the provided byte buffer.
 */
void BlockDataGenerator::copyAtlas(const util::TexturePacker<BlockRegistry::TextureId> &packer, 
        glm::ivec2 &size, std::vector<std::byte> &out) {
    const auto atlasSize = packer.getAtlasSize();
    XASSERT(atlasSize.x && atlasSize.y, "Invalid atlas size {}", atlasSize);

    // resize output buffer
    const size_t bytesPerPixel = 4 /* components */ * sizeof(float) /*bytes per*/;
    const size_t bytesPerRow = bytesPerPixel * atlasSize.x;

    const size_t numBytes = (atlasSize.x * atlasSize.y) * bytesPerPixel;

    out.resize(numBytes);
    size = atlasSize;

    // for each output texture, place it
    std::vector<float> textureBuffer;
    for(const auto &[textureId, bounds] : packer.getLayout()) {
#ifdef LOG_ATLAS_LAYOUT
        Logging::trace("Texture {} -> bounds {}", textureId, bounds);
#endif

        // get texture and yeet it into the buffer
        const auto &texture = this->registry->textures[textureId];

        textureBuffer.clear();
        textureBuffer.resize(texture.size.x * texture.size.y * 4, 0);

        texture.fillFunc(textureBuffer);

        // write pointer to the top left of the output
        const auto bytesPerTextureRow = texture.size.x * sizeof(float) * 4;
        std::byte *writePtr = out.data() + (bytesPerRow * bounds.y) + (bytesPerPixel * bounds.x);

        for(size_t y = 0; y < texture.size.y; y++) {
            // calculate offset into texture buffer and yeet it up
            const size_t textureYOff = y * texture.size.x * 4;
            memcpy(writePtr, textureBuffer.data() + textureYOff, bytesPerTextureRow);

            // advance write pointer
            writePtr += bytesPerRow;
        }
    }
}



/**
 * Builds the block appearance data texture in the provided buffer.
 *
 * This texture has a row for each appearance type; each row, in turn, currently has 16 columns
 * assigned to it. These are laid out as follows:
 * -  0...1: Bottom face texture coordinates
 * -  2...3: Top face exture coordinates
 * -  4...5: Side face texture coordinates (left)
 * -  6...7: Side face texture coordinates (right)
 * -  8...9: Side face texture coordinates (front)
 * - 10..11: Side face texture coordinates (back)
 * -     12: Material properties (x = specular, y = shininess)
 *
 * Note that we leave the first row devoid of all data. Appearance IDs start at 1, with air having
 * the "unofficial" ID of 0 even though it's not actually a block.
 */
void BlockDataGenerator::generate(glm::ivec2 &size, std::vector<glm::vec4> &out) {
    // determine required space and reserve it
    size = glm::ivec2(kDataColumns, this->registry->getNumRegistered()+2);
    out.resize(size.x * size.y, glm::vec4(0));

    // fill in data for each block
    std::lock_guard<std::mutex> lg(this->registry->appearancesLock);

    for(const auto &[id, appearance] : this->registry->appearances) {
        this->writeBlockInfo(out, id, appearance);
    }
}

/**
 * Writes block data for a the given block.
 */
void BlockDataGenerator::writeBlockInfo(std::vector<glm::vec4> &out, const size_t y, const BlockRegistry::BlockAppearanceType &appearance) {
    size_t off = (y * kDataColumns);
    glm::vec4 texUv;

    // UV for the bottom face
    texUv = this->blockAtlas.uvBoundsForTexture(appearance.texBottom);
    out[off + 0] = glm::vec4(texUv.x, texUv.w, texUv.z, texUv.w);
    out[off + 1] = glm::vec4(texUv.z, texUv.y, texUv.x, texUv.y);

    // UV coords for top face
    texUv = this->blockAtlas.uvBoundsForTexture(appearance.texTop);
    out[off + 2] = glm::vec4(texUv.x, texUv.w, texUv.z, texUv.w);
    out[off + 3] = glm::vec4(texUv.z, texUv.y, texUv.x, texUv.y);

    // UV coords for sides
    texUv = this->blockAtlas.uvBoundsForTexture(appearance.texSide);

    // left/right faces
    out[off + 4] = glm::vec4(texUv.x, texUv.w, texUv.x, texUv.y);
    out[off + 5] = glm::vec4(texUv.z, texUv.y, texUv.z, texUv.w);
    out[off + 6] = glm::vec4(texUv.z, texUv.w, texUv.z, texUv.y);
    out[off + 7] = glm::vec4(texUv.x, texUv.y, texUv.x, texUv.w);

    // front/back faces
    out[off + 8] = glm::vec4(texUv.x, texUv.y, texUv.z, texUv.y);
    out[off + 9] = glm::vec4(texUv.z, texUv.w, texUv.x, texUv.w);
    out[off + 10] = glm::vec4(texUv.x, texUv.w, texUv.z, texUv.w);
    out[off + 11] = glm::vec4(texUv.z, texUv.y, texUv.x, texUv.y);

    // material props
    out[off + 12] = glm::vec4(0.33, 0, 0, 0);
}

