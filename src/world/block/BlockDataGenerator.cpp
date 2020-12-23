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
void BlockDataGenerator::repackAtlas() {
    std::lock_guard<std::mutex> lg(this->registry->texturesLock);
    this->buildAtlasLayout();
}

/**
 * Lays out the textures of all blocks into the texture atlas.
 */
void BlockDataGenerator::buildTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out) {
    Logging::debug("Texture atlas format: RGBA{}", this->makeFloatAtlas ? "16F" : "8");

    // get texture lock
    std::lock_guard<std::mutex> lg(this->registry->texturesLock);

    // build atlas layout if needed
    if(this->atlasLayout.empty()) {
        Logging::debug("Rebuilding texture atlas...");
        this->buildAtlasLayout();
    }

    XASSERT(this->atlasSize.x && this->atlasSize.y, "Invalid atlas size {}", this->atlasSize);

    // resize output buffer
    const size_t bytesPerPixel = 4 /* components */ * (this->makeFloatAtlas ? sizeof(float) : 1) /*bytes per*/;
    const size_t bytesPerRow = bytesPerPixel * this->atlasSize.x;

    const size_t numBytes = (this->atlasSize.x * this->atlasSize.y) * bytesPerPixel;

    out.resize(numBytes);
    size = this->atlasSize;

    // for each output texture, place it
    std::vector<float> textureBuffer;
    for(const auto &[textureId, bounds] : this->atlasLayout) {
#ifdef LOG_ATLAS_LAYOUT
        Logging::trace("Texture {} -> bounds {}", textureId, bounds);
#endif

        // get texture and yeet it into the buffer
        const auto &texture = this->registry->textures[textureId];

        textureBuffer.clear();
        textureBuffer.resize(texture.size.x * texture.size.y * 4, 0);

        texture.fillFunc(textureBuffer);

        // copy it as-is if we want a floating point atlas
        if(this->makeFloatAtlas) {
            const auto bytesPerTextureRow = texture.size.x * sizeof(float) * 4;

            // write pointer to the top left of the output
            std::byte *writePtr = out.data() + (bytesPerRow * bounds.y) + (bytesPerPixel * bounds.x);

            for(size_t y = 0; y < texture.size.y; y++) {
                // calculate offset into texture buffer and yeet it up
                const size_t textureYOff = y * texture.size.x * 4;
                memcpy(writePtr, textureBuffer.data() + textureYOff, bytesPerTextureRow);

                // advance write pointer
                writePtr += bytesPerRow;
            }
        } 
        // otherwise, normalize [0, 1] to 0x00 to 0xFF. all HDR detail will be clipped
        else {
            XASSERT(false, "8 bit atlas not yet supported");
        }
    }
}

/**
 * Lays out all textures in the texture atlas.
 *
 * This algorithm works by first sorting all input textures in size (area, e.g. W x H) in
 * descending order. Then, iterating over this sorted list of textures, we check the output map
 * in each scanline see if there's enough width to fit the texture. If we find enough consecutive
 * lines to fit the texture, we set that as its bounding rect, and move on.
 *
 * Should we get to the end of the texture without finding a suitable location, the size of the
 * texture is expanded by a multiple of 64. In order to achieve a reasonably balanced texture size,
 * we will increment the width if it's less than the height, and vice versa.
 *
 * To optimize this algorithm, a vector indicating the index of the first "free" pixel on each
 * line is kept. This gets updated as we go.
 *
 * @note We assume the texture iteration lock is held already when entering this function.
 */
void BlockDataGenerator::buildAtlasLayout() {
    // remove existing and set the initial size
    std::unordered_map<BlockRegistry::TextureId, glm::ivec4> layout;
    glm::ivec2 size(32, 32);

    // sort all textures by total pixels (size)
    std::vector<std::pair<BlockRegistry::TextureId, size_t>> sizes;

    for(const auto &[textureId, info] : this->registry->textures) {
        // const size_t size = (info.size.x * info.size.y); 
        const size_t size = std::min(info.size.x, info.size.y); 
        sizes.emplace_back(textureId, size);
    }

    std::sort(std::begin(sizes), std::end(sizes), [](const auto &l, const auto &r) {
        return (l.second > r.second);
    });

    // fitting process for all textures
    std::vector<size_t> freeVec;
    freeVec.resize(size.y, 0);

    for(const auto &pair : sizes) {
        const auto [textureId, area] = pair;
        // get the texture
        const auto &info = this->registry->textures[textureId]; 

        size_t tries = 0;

        // set up to iterate through the entire height of the texture
again:;
        bool startedOrigin = false;
        bool foundSection = false;
        glm::ivec2 origin(0, 0);

        // try to find a consecutive range of free pixels
        for(size_t y = 0; y < size.y; y++) {
            const size_t freeThisLine = size.x - freeVec[y];

            // check to see if we can start our bounding rect here
            if(!startedOrigin) {
                // yep! we have at least enough space as this texture is wind
                if(freeThisLine >= info.size.x) {
                    origin = glm::ivec2(freeVec[y], y);
                    startedOrigin = true;
                }
            }
            // we've started a bounding rect already. see if we've got at least W pixels free still
            else {
                // insufficient space on this line, so check next line if we can fit there
                if(freeThisLine < info.size.x || origin.x < freeVec[y]) {
                    startedOrigin = false;
                }
                // check to see if we've found height consecutive pixels
                else {
                    const size_t consecutiveLines = y - origin.y;

                    // we've found H consecutive lines with enough space, so we can place the texture
                    if(consecutiveLines == info.size.y) {
                        foundSection = true;
                        goto dispensary;
                    }
                }
            }
        }

dispensary:;
        // we've found a section to place this texture, yeet it
        if(foundSection) {
            // update the "first free pixel" vector
            for(size_t y = 0; y < info.size.y; y++) {
                freeVec[origin.y + y] = origin.x + info.size.x;
            }

            // add the bounds and go on to the next texture
            glm::ivec4 bounds(origin, info.size);
            layout[textureId] = bounds;

#ifdef LOG_ATLAS_LAYOUT
            Logging::trace("LAYOUT: Bounds for texture {}: {}", textureId, bounds);
#endif
        }
        // otherwise, once we get here, we should resize the texture
        else {
            // if we get here, the texture should be resized
            constexpr static const size_t kGrowStep = 64;

            if(size.x >= size.y) {
                const size_t smaller = (info.size.y / kGrowStep) * kGrowStep;
                size_t toAdd = smaller + kGrowStep;
                size.y += toAdd;

                freeVec.resize(size.y, 0);
            } else {
                const size_t smaller = (info.size.x / kGrowStep) * kGrowStep;
                size_t toAdd = smaller + kGrowStep;
                size.x += toAdd;
            }

            if(++tries > 4) {
                throw std::runtime_error("Failed to fit texture!");
            }

            // once resized, try to place the texture again
#ifdef LOG_ATLAS_LAYOUT
            Logging::trace("LAYOUT: Resized atlas: {}", size);
#endif
            goto again;
        }
    }

    // everything was placed, so set the new data
    XASSERT(layout.size() >= this->registry->textures.size(), "Failed to place all textures");

    this->atlasLayout = layout;
    this->atlasSize = size;
}

/**
 * Gets the bounding rect, in UV coordinates, for a particular texture in the texture atlas.
 *
 * The rect is in the (top left, bottom right) format. It is converted from the (top left, size)
 * format the atlas represents textures as internallly.
 */
glm::vec4 BlockDataGenerator::uvBoundsForTexture(BlockRegistry::TextureId id) {
    if(!id) {
        return glm::vec4(0);
    }

    const auto bounds = this->atlasLayout.at(id);
    const auto topLeft = glm::vec2(bounds.x, bounds.y);
    const auto size = glm::vec2(bounds.z, bounds.w);

    return glm::vec4(topLeft, topLeft + size) / glm::vec4(this->atlasSize, this->atlasSize);
}



/**
 * Builds the block appearance data texture in the provided buffer.
 *
 * This texture has a row for each appearance type; each row, in turn, currently has 16 columns
 * assigned to it. These are laid out as follows:
 * -  0...1: Bottom face texture coordinates
 * -  2...3: Top face exture coordinates
 * -  4...5: Side faces texture coordinates
 * -      6: Material properties (x = specular, y = shininess)
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

    size_t y = 1;
    for(const auto &[id, appearance] : this->registry->appearances) {
        this->writeBlockInfo(out, y, appearance);
        y++;
    }
}

/**
 * Writes block data for a the given block.
 */
void BlockDataGenerator::writeBlockInfo(std::vector<glm::vec4> &out, const size_t y, const BlockRegistry::BlockAppearanceType &appearance) {
    size_t off = (y * kDataColumns);
    glm::vec4 texUv;

    // UV for the bottom face
    texUv = this->uvBoundsForTexture(appearance.texBottom);
    out[off + 0] = glm::vec4(texUv.x, texUv.w, texUv.z, texUv.w);
    out[off + 1] = glm::vec4(texUv.z, texUv.y, texUv.x, texUv.y);

    // UV coords for top face
    texUv = this->uvBoundsForTexture(appearance.texTop);
    out[off + 2] = glm::vec4(texUv.x, texUv.w, texUv.z, texUv.w);
    out[off + 3] = glm::vec4(texUv.z, texUv.y, texUv.x, texUv.y);

    // UV coords for sides
    texUv = this->uvBoundsForTexture(appearance.texSide);
    out[off + 4] = glm::vec4(texUv.x, texUv.w, texUv.z, texUv.w);
    out[off + 5] = glm::vec4(texUv.z, texUv.y, texUv.x, texUv.y);

    // material props
    out[off + 6] = glm::vec4(0.33, 0, 0, 0);
}

