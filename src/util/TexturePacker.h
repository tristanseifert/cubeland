/**
 * Provides a general image packing algorithm. It finds a reasonably efficient way to pack a bunch
 * of 2D rectangles of image data into one larger image.
 */
#ifndef UTIL_TEXTUREPACKER_H
#define UTIL_TEXTUREPACKER_H

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <cstddef>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <Logging.h>

namespace util {
template<typename T> class TexturePacker {
    public:
        /// Returns the image size required to fit all textures in the packer.
        glm::ivec2 getAtlasSize() const {
            return this->atlasSize;
        }

        /**
         * Returns UV coords (top/left and bottom/right) of the given texture.
         */
        glm::vec4 uvBoundsForTexture(T id) {
            const auto bounds = this->atlasLayout.at(id);
            const auto topLeft = glm::vec2(bounds.x, bounds.y);
            const auto size = glm::vec2(bounds.z, bounds.w);

            return glm::vec4(topLeft, topLeft + size) / glm::vec4(this->atlasSize, this->atlasSize);
        }

        /**
         * Updates the layout of the texture atlas.
         */
        void updateLayout(const std::unordered_map<T, glm::ivec2> &textures) {
            this->atlasLayout.clear();
            this->buildAtlasLayout(textures);
        }

        /**
         * Returns a reference to the layout of the atlas.
         */
        const std::unordered_map<T, glm::ivec4> &getLayout() const {
            return this->atlasLayout;
        }

    private:
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
         * @param textures Mapping of texture id -> pixel size, used to lay out the textures.
         */
        void buildAtlasLayout(const std::unordered_map<T, glm::ivec2> &textures) {
            // remove existing and set the initial size
            std::unordered_map<T, glm::ivec4> layout;
            glm::ivec2 size(32, 32);

            // sort all textures by total pixels (size)
            std::vector<std::pair<T, size_t>> sizes;

            for(const auto &[textureId, size] : textures) {
                // const size_t pixels = (info.size.x * info.size.y); 
                const size_t pixels = std::min(size.x, size.y); 
                sizes.emplace_back(textureId, pixels);
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
                const auto textureSize = textures.at(textureId);

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
                        if(freeThisLine >= textureSize.x) {
                            origin = glm::ivec2(freeVec[y], y);
                            startedOrigin = true;
                        }
                    }
                    // we've started a bounding rect already. see if we've got at least W pixels free still
                    else {
                        // insufficient space on this line, so check next line if we can fit there
                        if(freeThisLine < textureSize.x || origin.x < freeVec[y]) {
                            startedOrigin = false;
                        }
                        // check to see if we've found height consecutive pixels
                        else {
                            const size_t consecutiveLines = y - origin.y;

                            // we've found H consecutive lines with enough space, so we can place the texture
                            if(consecutiveLines == textureSize.y) {
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
                    for(size_t y = 0; y < textureSize.y; y++) {
                        freeVec[origin.y + y] = origin.x + textureSize.x;
                    }

                    // add the bounds and go on to the next texture
                    glm::ivec4 bounds(origin, textureSize);
                    layout[textureId] = bounds;
                }
                // otherwise, once we get here, we should resize the texture
                else {
                    // if we get here, the texture should be resized
                    constexpr static const size_t kGrowStep = 64;

                    if(size.x >= size.y) {
                        const size_t smaller = (textureSize.y / kGrowStep) * kGrowStep;
                        size_t toAdd = smaller + kGrowStep;
                        size.y += toAdd;

                        freeVec.resize(size.y, 0);
                    } else {
                        const size_t smaller = (textureSize.x / kGrowStep) * kGrowStep;
                        size_t toAdd = smaller + kGrowStep;
                        size.x += toAdd;
                    }

                    if(++tries > 4) {
                        throw std::runtime_error("Failed to fit texture!");
                    }

                    // once resized, try to place the texture again
                    goto again;
                }
            }

            // try to shrink the Y edge
            size_t pixelsToShrink = 0;
            for(int y = (size.y - 1); y >= 0; y--) {
                if(freeVec[y] == 0) {
                    pixelsToShrink++;
                } else {
                    goto noMoreYShrink;
                }
            }

noMoreYShrink:;
            size.y -= pixelsToShrink;

            // try to shrink the X edge
            size_t xPixelsUsed = 0;
            for(size_t y = 0; y < size.y; y++) {
                xPixelsUsed = std::max(xPixelsUsed, freeVec[y]);
            }
            size.x = xPixelsUsed;

            // everything was placed, so set the new data
            this->atlasLayout = layout;
            this->atlasSize = size;
        }
    private:
        /// mapping of texture id -> bounding rect in the texture atlas
        std::unordered_map<T, glm::ivec4> atlasLayout;
        /// size of the texture atlas
        glm::ivec2 atlasSize;
};
}

#endif
