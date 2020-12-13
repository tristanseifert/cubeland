/*
 * TextureCube.h
 *
 * A cubemap texture, e.g. one with six faces.
 *
 *  Created on: Aug 24, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_TEXTURE_TEXTURECUBE_H_
#define GFX_BUFFER_TEXTURE_TEXTURECUBE_H_

#include "Texture.h"

#include <string>
#include <vector>

namespace gfx {
class TextureCube: public Texture {
    public:
        TextureCube(int unit);
        TextureCube() : TextureCube(0) {}

        ~TextureCube();

        void bind(void);
        static void unbind(void);

        void dump(const std::string &base);

        void allocateBlank(unsigned int width, unsigned int height, TextureFormat format);
        void bufferSubData(unsigned int width, unsigned int height, unsigned int xOff,
                           unsigned int yOff, TextureFormat format, void *data);

        void loadFromImages(const std::vector<std::string> &paths, bool sRGB = false);
};
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE_TEXTURECUBE_H_ */
