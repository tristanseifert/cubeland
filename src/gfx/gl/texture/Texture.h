#ifndef GFX_BUFFER_TEXTURE_TEXTURE_H_
#define GFX_BUFFER_TEXTURE_TEXTURE_H_

#include <cstddef>
#include <string>
#include <vector>

#include <glbinding/gl/gl.h>

#include <glm/glm.hpp>

namespace gfx {
class Texture {
    public:
        /**
         * Various texture formats.
         *
         * The first three or four characters represent the order of the
         * components, followed by the bit depth per component, and "F" if
         * the type is floating point.
         */
        enum TextureFormat {
            RED8 = 1,
            RED16F,
            RED32F,

            RG8,
            RG16F,
            RG32F,

            RGB,
            RGB8,
            RGB16F,
            RGB32F,

            RGBA,
            RGBA8,
            RGBA16F,
            RGBA32F,

            DepthGeneric,
            Depth24Stencil8,

            Unknown = -1
        };

        /**
         * Various texture formats, as stored on disk.
         *
         * These encompass most of the general compressed formats (i.e.
         * JPEG, PNG, etc) and other, more specific (DXT1, 3, and 5)
         * formats.
         */
        enum TextureLoadFormat {
            Uncompressed, // raw binary data
            Compressed, // i.e. JPEG

            // formats sent as compressed data to the GPU
            DXT1,
            DXT3,
            DXT5
        };

        /**
         * Various texture wrap modes.
         */
        enum WrapMode {
            Clamp,
            ClampToBorder,
            ClampToEdge,
            MirroredRepeat,
            Repeat
        };

    public:
        Texture(int unit);
        Texture() : Texture(0) {}

        virtual ~Texture();

        virtual void bind(void) = 0;
        static void unbind(void);

        virtual void dump(const std::string &base) = 0;

        void setBorderColour(glm::vec4 border);

        TextureFormat getFormat(void) {
            return this->format;
        }
        const size_t getWidth(void) {
            return this->width;
        }
        const size_t getHeight(void) {
            return this->height;
        }

        void setDebugName(const std::string str) {
            this->debugName = str;
        }

        gl::GLuint getGlObjectId() const {
            return this->texture;
        }

    public:
        int unit;

        // This should not be accessed from external code.
        gl::GLuint texture;

        // functions for getting GL enums
    protected:
        gl::GLenum glFormat(void);
        gl::GLenum glWrapMode(WrapMode mode);
        gl::GLenum glTypeForLoadFormat(bool sRGB);

        // functions for loading image data
    protected:
        void *loadImageData(const std::string &path, int *width, int *height, gl::GLenum *format);
        void releaseImageData(void *data);

        int loadDDSFile(std::string path);
        void releaseDDSFile(void);

        std::vector<char> *getDDSData() {
            return this->ddsData;
        }

    protected:
        TextureFormat format = Unknown;
        TextureLoadFormat loadedFormat = Uncompressed;

        size_t width = 0;
        size_t height = 0;
        unsigned int mipMapCount = 0;

        WrapMode wrapS = Clamp, wrapT = Clamp, wrapR = Clamp;
        glm::vec4 borderColour = glm::vec4(1, 0, 1, 1);

        std::vector<std::string> loadPaths;

        std::string debugName = "UntitledTexture";

    private:
        std::vector<char> *ddsData = NULL;
};
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE_TEXTURE_H_ */
