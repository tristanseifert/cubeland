#include "PixelBuffer.h"
#include "../texture/Texture2D.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace std;
using namespace gl;
using namespace gfx;

/**
 * Allocates a new pixel buffer, which transfers to the specified texture.
 */
PixelBuffer::PixelBuffer(std::shared_ptr<Texture2D> tex) : texture(tex) {
    XASSERT(tex != nullptr, "Invalid texture");

    // allocate a pixel buffer
    glGenBuffersARB(1, &this->pbo);
}

PixelBuffer::~PixelBuffer() {
    // clean up
    glDeleteBuffersARB(1, &this->pbo);
}

/**
 * Returns a buffer, with the given size, into which data can be written. This
 * buffer is mapped memory directly from the OpenGL driver.
 *
 * @note The buffer is write only.
 */
void *PixelBuffer::getBuffer(size_t size) {
    // keep state
    XASSERT(++this->bufferCount >= 1, "Invalid buffer count");

    // allocate the new buffer
    bind();
    glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, (GLsizeiptrARB) size, nullptr, GL_STREAM_DRAW_ARB);

    // attempt to map the buffer
    void* ptr = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
    XASSERT(ptr != nullptr, "glMapBufferARB() failed");

    // unbind
    PixelBuffer::unbind();
    return ptr;
}

/**
 * Copies the specified data into the buffer directly.
 */
void PixelBuffer::bufferData(void *data, size_t size) {
    // stick data into the buffer without mapping it
    bind();
    glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, (GLsizeiptrARB) size, data, GL_STREAM_DRAW_ARB);

    // transfer to texture
    this->texture->bind();

    Texture::TextureFormat format = this->texture->getFormat();
    this->texture->bufferSubData(this->texture->getWidth(), this->texture->getHeight(), 0, 0,
                                 format, nullptr);

    // done
    PixelBuffer::unbind();
}

/**
 * Releases the previously allocated buffer to the GPU for immediate transfer.
 */
void PixelBuffer::releaseBuffer(void) {
    // check that there are buffers to release
    XASSERT(--this->bufferCount == 0, "No buffers to release");

    // unmap the previous buffer
    bind();
    glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);

    // transfer to texture
    this->texture->bind();

    Texture::TextureFormat format = this->texture->getFormat();
    this->texture->bufferSubData(this->texture->getWidth(), this->texture->getHeight(), 0, 0,
                                 format, nullptr);

    // unbind
    PixelBuffer::unbind();
}

/**
 * Binds the pixel buffer.
 */
void PixelBuffer::bind(void) {
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, this->pbo);
}

/**
 * Unbinds a currently active pixel buffer.
 */
void PixelBuffer::unbind(void) {
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
}
