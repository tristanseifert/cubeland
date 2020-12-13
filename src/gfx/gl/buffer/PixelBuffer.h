/*
 * PixelBuffer.h
 *
 * Wraps an OpenGL pixel buffer object, which can be used to stream new texture
 * data to VRAM efficiently.
 *
 *  Created on: Aug 29, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_TEXTURE_PIXELBUFFER_H_
#define GFX_BUFFER_TEXTURE_PIXELBUFFER_H_

#include <memory>

#include <glbinding/gl/gl.h>

namespace gfx {
class Texture;

class PixelBuffer {
    public:
        PixelBuffer(std::shared_ptr<Texture> tex);
        virtual ~PixelBuffer();

        void *getBuffer(size_t size);
        void releaseBuffer(void);
        void bufferData(void *data, size_t size);

    private:
        void bind(void);
        static void unbind(void);

    private:
        unsigned int bufferCount = 0;

        gl::GLuint pbo;

        // texture backing the pixel buffer
        std::shared_ptr<Texture> texture = nullptr;
    };
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE_PIXELBUFFER_H_ */
