#ifndef GFX_BUFFER_TEXTURE1D_H_
#define GFX_BUFFER_TEXTURE1D_H_

#include "Texture.h"

#include <glbinding/gl/gl.h>

#include <string>

namespace gfx {
class Texture1D: public Texture {
    public:
        Texture1D(int unit, bool bind);
        Texture1D(int unit) : Texture1D(unit, true) {}
        Texture1D() : Texture1D(0, true) {}

        ~Texture1D();

        void bind(void);
        static void unbind(void);

        void dump(const std::string &base);

        void allocateBlank(const size_t width, const TextureFormat format);
        void bufferSubData(const size_t width, const size_t xOff, const TextureFormat format, const void *data);

        void loadFromImage(const std::string &path, bool sRGB = false);

        void setUsesLinearFiltering(bool enabled);

        void setWrapMode(WrapMode s, WrapMode t);
        void setBorderColor(const glm::vec4 &border);

    private:
        bool usesLinearFiltering = true;
};
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE2D_H_ */
