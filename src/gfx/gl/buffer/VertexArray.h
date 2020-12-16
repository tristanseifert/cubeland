/*
 * VertexArray.h
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_VERTEXARRAY_H_
#define GFX_BUFFER_VERTEXARRAY_H_

#include <glbinding/gl/gl.h>

namespace gfx {
	class VertexArray {
		public:
			enum VertexAttribType {
				Byte,
				UnsignedByte,
				Short,
				UnsignedShort,
				Integer,
				UnsignedInteger,

				HalfFloat,
				Float,
				Double,
				Fixed
			};

		public:
			VertexArray();
			~VertexArray();

			void bind(void);
			static void unbind(void);

			void registerVertexAttribPointer(gl::GLuint index, gl::GLint size,
											 VertexAttribType type,
											 gl::GLsizei stride,
											 std::size_t offset,
                                                                                         gl::GLuint divisor = 0);
		private:
			static gl::GLenum attribTypeGL(VertexAttribType size);

		private:
			gl::GLuint vao;
	};
} /* namespace gfx */

#endif /* GFX_BUFFER_VERTEXARRAY_H_ */
