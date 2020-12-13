/*
 * RenderBuffer.cpp
 *
 *  Created on: Aug 22, 2015
 *      Author: tristan
 */

#include "RenderBuffer.h"

#include <iostream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace std;
using namespace gl;

namespace gfx {

/**
 * Allocates an OpenGL render buffer.
 */
RenderBuffer::RenderBuffer(unsigned int width, unsigned int height) {
	// store size
	this->width = width;
	this->height = height;

	// allocate RBO
	glGenRenderbuffers(1, &this->rbo);
}

/**
 * Cleans up any allocated resources.
 */
RenderBuffer::~RenderBuffer() {
	// delete RBO
	glDeleteRenderbuffers(1, &this->rbo);
}

/**
 * Binds this render buffer.
 */
void RenderBuffer::bind(void) {
	glBindRenderbuffer(GL_RENDERBUFFER, this->rbo);
}

/**
 * Unbinds the currently bound render buffer.
 */
void RenderBuffer::unbind(void) {
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

/**
 * Allocates storage for the render buffer, using a 4bpp format, where 24 bits
 * are dedicated to depth, and the remaining eight to a stencil buffer.
 */
void RenderBuffer::allocateDepth() {
	this->bind();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
    					  (GLsizei) this->width, (GLsizei) this->height);
}

} /* namespace gfx */
