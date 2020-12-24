/**
 * A simple class that loads PNG textures for blocks. It handles the conversion to floating point
 * values as well.
 */
#ifndef WORLD_BLOCK_TEXTURELOADER_H
#define WORLD_BLOCK_TEXTURELOADER_H

#include <string>
#include <vector>

namespace world {
class TextureLoader {
    public:
        static void load(const std::string &path, std::vector<float> &out, const bool gammaConvert = false);
};
}

#endif
