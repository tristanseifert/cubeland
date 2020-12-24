/*
 * FrameBuffer.h
 *
 * Wraps a standard OpenGL framebuffer.
 *
 *  Created on: Aug 22, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_FRAMEBUFFER_H_
#define GFX_BUFFER_FRAMEBUFFER_H_

#include <memory>
#include <vector>

#include <glbinding/gl/gl.h>

namespace gfx {
class RenderBuffer;
class Texture2D;

class FrameBuffer {
    public:
        enum AttachmentType {
            ColourAttachment0 = 1,
            ColourAttachment1 = 2,
            ColourAttachment2 = 3,
            ColourAttachment3 = 4,
            ColourAttachment4 = 5,
            ColourAttachment5 = 6,
            ColourAttachment6 = 7,
            ColourAttachment7 = 8,

            Depth,
            Stencil,
            DepthStencil,

            End = 0xFFFF
        };

    public:
        FrameBuffer();
        ~FrameBuffer();

        static bool isComplete(void);

        void bindRW(void);
        static void unbindRW(void);
        void bindR(void);
        static void unbindR(void);
        void bindW(void);
        static void unbindW(void);

        void attachTexture2D(Texture2D *tex, AttachmentType attachment);

        void attachTexture2D(std::shared_ptr<Texture2D> tex, AttachmentType attachment);
        void attachTextureRect(std::shared_ptr<Texture2D> tex, AttachmentType attachment);
        void attachRenderBuffer(std::shared_ptr<RenderBuffer> buf, AttachmentType attachment);

        void setDrawBuffers(AttachmentType attachments[]);

        void drawBuffersWithoutColour(void);

        static gl::GLint currentDrawBuffer(void);
        static void bindDrawBufferByName(gl::GLint n);

    protected:
        static gl::GLenum glAttachmentType(AttachmentType type);

    private:
        gl::GLuint framebuffer;

        std::vector<std::shared_ptr<Texture2D>> textures;
        std::vector<std::shared_ptr<RenderBuffer>> renderBuffers;
};
} /* namespace gfx */

#endif /* GFX_BUFFER_FRAMEBUFFER_H_ */
