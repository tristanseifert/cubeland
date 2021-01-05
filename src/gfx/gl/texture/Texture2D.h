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

        void allocateBlank(const size_t width, const size_t height, const TextureFormat format);

        void bufferSubData(const size_t width, const size_t height, const size_t xOff,
                           const size_t yOff, const TextureFormat format, const void *data);

        void loadFromImage(const std::string &path, bool sRGB = false);

        void setUsesLinearFiltering(bool enabled);

        void setWrapMode(WrapMode s, WrapMode t);
        void setBorderColour(glm::vec4 border);

        void generateMipMaps(void);

    private:
        bool usesLinearFiltering = true;
};
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE2D_H_ */
