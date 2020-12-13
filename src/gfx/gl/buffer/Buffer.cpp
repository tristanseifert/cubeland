/*
 * Buffer.cpp
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#include "Buffer.h"

#include <iostream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace std;
using namespace gl;
namespace gfx {

/**
 * Generates the buffer object.
 */
Buffer::Buffer(BufferType type, BufferUsage usage, BufferMapPolicy policy) {
	// allocate a buffer
	glGenBuffers(1, &this->buffer);

	// store some state
	this->type = type;
	this->usage = usage;
	this->policy = policy;
}
/**
 * Destroys the buffer object.
 */
Buffer::~Buffer() {
	glDeleteBuffers(1, &this->buffer);
}

/**
 * Converts our internal buffer type into the equivalent OpenGL enum.
 */
GLenum Buffer::bufferTypeGL(BufferType type) {
	switch(type) {
		case Array:
			return GL_ARRAY_BUFFER;

		case ElementArray:
			return GL_ELEMENT_ARRAY_BUFFER;
	}
}

/**
 * Converts our internal buffer usage hint into the equivalent OpenGL enum.
 */
GLenum Buffer::usageTypeGL(BufferUsage usage) {
	switch(usage) {
		case StreamDraw:
			return GL_STREAM_DRAW;
		case StreamRead:
			return GL_STREAM_READ;
		case StreamCopy:
			return GL_STREAM_COPY;

		case StaticDraw:
			return GL_STATIC_DRAW;
		case StaticRead:
			return GL_STATIC_READ;
		case StaticCopy:
			return GL_STATIC_COPY;

		case DynamicDraw:
			return GL_DYNAMIC_DRAW;
		case DynamicRead:
			return GL_DYNAMIC_READ;
		case DynamicCopy:
			return GL_DYNAMIC_COPY;
	}
}

/**
 * Converts our internal buffer map policy into the equivalent OpenGL enum.
 */
GLenum Buffer::bufferMapPolicy(BufferMapPolicy policy) {
	switch(policy) {
		case ReadOnly:
			return GL_READ_ONLY;
		case WriteOnly:
			return GL_WRITE_ONLY;
		case ReadWrite:
			return GL_READ_WRITE;
	}
}

/**
 * Binds the buffer.
 */
void Buffer::bind(void) {
	glBindBuffer(bufferTypeGL(), this->buffer);
}

/**
 * Unbinds a buffer with the same type as this buffer. It may not neccisarily
 * unbind the current buffer.
 */
void Buffer::unbind(void) {
	glBindBuffer(bufferTypeGL(), 0);
}

/**
 * Unbinds the buffer of the specified type.
 */
void Buffer::unbind(BufferType type) {
	glBindBuffer(bufferTypeGL(type), 0);
}

/**
 * Copies the input data into the buffer.
 */
void Buffer::bufferData(std::size_t size, void *data) {
	this->bind();
	glBufferData(bufferTypeGL(), (GLsizeiptr) size, data, usageTypeGL());
}

/**
 * Replaces a subset of the buffer's data with the given data.
 */
void Buffer::replaceData(std::size_t offset, std::size_t size, void *data) {
	this->bind();
	glBufferSubData(bufferTypeGL(), (GLintptr) offset, (GLsizeiptr) size, data);
}

/**
 * Reserves the specified amount of memory in the buffer.
 */
void Buffer::reserveData(std::size_t size) {
	this->bind();
	glBufferData(bufferTypeGL(), (GLsizeiptr) size, NULL, usageTypeGL());
}

/**
 * Attempts to map the buffer into virtual memory, using the specified policy.
 * This call may fail for some reason, and if it does, NULL is returned.
 *
 * @note The buffer must be unmapped before it is used for rendering.
 */
void *Buffer::mapBuffer(BufferMapPolicy policy) {
	// attempt to map the buffer
	return glMapBuffer(bufferTypeGL(), bufferMapPolicy(policy));
}

/**
 * Unmaps the buffer, if it has been previously mapped.
 */
void Buffer::unmapBuffer() {
	glUnmapBuffer(bufferTypeGL());
}

} /* namespace gfx */
