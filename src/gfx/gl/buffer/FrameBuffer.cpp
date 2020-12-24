/*
 * FrameBuffer.cpp
 *
 *  Created on: Aug 22, 2015
 *      Author: tristan
 */

#include "FrameBuffer.h"
#include "RenderBuffer.h"
#include "gfx/gl/texture/Texture2D.h"

#include <iostream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <Logging.h>

using namespace std;
using namespace gl;
using namespace gfx;

/**
 * Allocates the OpenGL framebuffer.
 */
FrameBuffer::FrameBuffer() {
    // allocate buffer
    glGenFramebuffers(1, &this->framebuffer);
}

/**
 * Clean up the resources held by the framebuffer.
 */
FrameBuffer::~FrameBuffer() {
    // delete the framebuffer
    glDeleteFramebuffers(1, &this->framebuffer);
}

/**
 * Checks whether the currently bound framebuffer is complete.
 */
bool FrameBuffer::isComplete(void) {
    // check status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        Logging::error("Invalid framebuffer status {}", status);
    }
    return (status == GL_FRAMEBUFFER_COMPLETE);
}

/**
 * Binds this framebuffer as both the read and write target, making it active.
 */
void FrameBuffer::bindRW(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
}

/**
 * Unbinds the framebuffer(s) currently set as the read and write targets.
 */
void FrameBuffer::unbindRW(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * Binds this framebuffer as both the read and write target, making it active.
 */
void FrameBuffer::bindR(void) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, this->framebuffer);
}

/**
 * Unbinds the framebuffer(s) currently set as the read and write targets.
 */
void FrameBuffer::unbindR(void) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

/**
 * Binds this framebuffer as both the read and write target, making it active.
 */
void FrameBuffer::bindW(void) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->framebuffer);
}

/**
 * Unbinds the framebuffer(s) currently set as the read and write targets.
 */
void FrameBuffer::unbindW(void) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

/**
 * Attaches the specified texture to the framebuffer.
 */
void FrameBuffer::attachTexture2D(std::shared_ptr<Texture2D> tex, AttachmentType attachment) {
    // bind buffer
    bindRW();

    this->textures.push_back(tex);

    // attach texture
    GLenum type = FrameBuffer::glAttachmentType(attachment);
    glFramebufferTexture2D(GL_FRAMEBUFFER, type, GL_TEXTURE_2D, tex->texture, 0);
}
void FrameBuffer::attachTexture2D(Texture2D *tex, AttachmentType attachment) {
    // bind buffer
    bindRW();

    // attach texture
    GLenum type = FrameBuffer::glAttachmentType(attachment);
    glFramebufferTexture2D(GL_FRAMEBUFFER, type, GL_TEXTURE_2D, tex->texture, 0);
}

/**
 * Attaches the specified texture to the framebuffer.
 */
void FrameBuffer::attachTextureRect(std::shared_ptr<Texture2D> tex, AttachmentType attachment) {
    // bind buffer
    bindRW();

    this->textures.push_back(tex);

    // attach texture
    GLenum type = FrameBuffer::glAttachmentType(attachment);
    glFramebufferTexture2D(GL_FRAMEBUFFER, type, GL_TEXTURE_RECTANGLE_ARB, tex->texture, 0);
}

/**
 * Attaches the specified render buffer to the framebuffer.
 */
void FrameBuffer::attachRenderBuffer(std::shared_ptr<RenderBuffer> buf, AttachmentType attachment) {
    // bind buffer
    bindRW();

    this->renderBuffers.push_back(buf);

    // attach buffer
    GLenum type = FrameBuffer::glAttachmentType(attachment);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, type, GL_RENDERBUFFER, buf->rbo);
}

/**
 * Sets the draw buffers to be used for this framebuffer, i.e. where fragment
 * shaders output/input is read to/from.
 *
 * @note Terminate the list with the AttachmentType::End value.
 */
void FrameBuffer::setDrawBuffers(AttachmentType attachments[]) {
    // figure out how many buffers there are
    size_t count = 0;

    while(true) {
        if(attachments[count] != FrameBuffer::End) {
            count++;
        } else {
            break;
        }
    }

    // convert the array pls.
    GLenum buffers[count];

    for(size_t i = 0; i < count; i++) {
        buffers[i] = FrameBuffer::glAttachmentType(attachments[i]);
    }

    // Send it off to the framebuffer
    bindRW();
    glDrawBuffers((GLsizei) count, buffers);
}

/**
 * Sets the framebuffer, which is currently bound, to not use any colour
 * attachments.
 */
void FrameBuffer::drawBuffersWithoutColour(void) {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
}

/**
 * Converts our attachment type to the OpenGL type.
 */
GLenum FrameBuffer::glAttachmentType(AttachmentType type) {
    switch(type) {
        case ColourAttachment0:
        case ColourAttachment1:
        case ColourAttachment2:
        case ColourAttachment3:
        case ColourAttachment4:
        case ColourAttachment5:
        case ColourAttachment6:
        case ColourAttachment7:
            return GL_COLOR_ATTACHMENT0 + (type - 1);
            break;

        case Depth:
            return GL_DEPTH_ATTACHMENT;

        case Stencil:
            return GL_STENCIL_ATTACHMENT;

        case DepthStencil:
            return GL_DEPTH_STENCIL_ATTACHMENT;

        default:
            return GL_DEPTH_ATTACHMENT;
    }

    return (GLenum) -1;
}

/**
 * Returns the currently bound draw framebuffer.
 */
GLint FrameBuffer::currentDrawBuffer(void) {
    GLint drawFboId = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);

    return drawFboId;
}

/**
 * Binds a framebuffer by its GL name.
 */
void FrameBuffer::bindDrawBufferByName(GLint n) {
    glBindFramebuffer(GL_FRAMEBUFFER, n);
}

