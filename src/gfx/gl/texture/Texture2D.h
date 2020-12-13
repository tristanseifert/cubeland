/*
 * Texture2D.h
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_TEXTURE2D_H_
#define GFX_BUFFER_TEXTURE2D_H_

#include "Texture.h"

#include <glbinding/gl/gl.h>

#include <string>

namespace gfx {
class Texture2D: public Texture {
    public:
        Texture2D(int unit, bool bind);
        Texture2D(int unit) : Texture2D(unit, true) {}
        Texture2D() : Texture2D(0, true) {}

        ~Texture2D();

        void bind(void);
        static void unbind(void);

        void dump(const std::string &base);

        void allocateBlank(unsigned int width, unsigned int height, TextureFormat format);

        void bufferSubData(unsigned int width, unsigned int height, unsigned int xOff,
                           unsigned int yOff, TextureFormat format, void *data);

        void loadFromImage(const std::string &path, bool sRGB = false);

        void setUsesLinearFiltering(bool enabled);

        void setWrapMode(WrapMode s, WrapMode t);
        void setBorderColour(glm::vec4 border);

        void generateMipMaps(void);
};
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE2D_H_ */
