/*
 * VertexArray.cpp
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#include "VertexArray.h"

#include <iostream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace std;
using namespace gl;
namespace gfx {

/**
 * Allocates a vertex array object.
 */
VertexArray::VertexArray() {
	glGenVertexArrays(1, &this->vao);
}

/**
 * Deallocates the VAO.
 */
VertexArray::~VertexArray() {
	glDeleteVertexArrays(1, &this->vao);
}

/**
 * Converts the internal representation of a vertex attribute pointer's data
 * size to the OpenGL analogue.
 */
GLenum VertexArray::attribTypeGL(VertexAttribType size) {
	switch(size) {
		case Byte:
			return GL_BYTE;
		case UnsignedByte:
			return GL_UNSIGNED_BYTE;
		case Short:
			return GL_SHORT;
		case UnsignedShort:
			return GL_UNSIGNED_SHORT;
		case Integer:
			return GL_INT;
		case UnsignedInteger:
			return GL_UNSIGNED_INT;

		case HalfFloat:
			return GL_HALF_FLOAT;
		case Float:
			return GL_FLOAT;
		case Double:
			return GL_DOUBLE;
		case Fixed:
			return GL_FIXED;
	}

	// we should not get down here
	return (GLenum) 0;
}

/**
 * Binds the VAO.
 */
void VertexArray::bind(void) {
	glBindVertexArray(this->vao);
}

/**
 * Breaks the existing vertex array association.
 */
void VertexArray::unbind(void) {
	glBindVertexArray(0);
}

/**
 * Registers a vertex attribute pointer, given an attribute index.
 *
 * @note The vertex attribute array is enabled for the given index.
 *
 * @param index Index of the attribute
 * @param size Number of components, between 1 and 4.
 * @param type Type of data
 * @param stride Number of bytes between consecutive entries.
 * @param offset Offset into the data array.
 */
void VertexArray::registerVertexAttribPointer(GLuint index, GLint size,
											  VertexAttribType type,
											  GLsizei stride, size_t offset) {
	// bind the VAO
	this->bind();

	// enable the array
	glEnableVertexAttribArray(index);

	// register the pointer
	glVertexAttribPointer(index, size, attribTypeGL(type), GL_FALSE, stride,
						  (void *) offset);
}

} /* namespace gfx */
