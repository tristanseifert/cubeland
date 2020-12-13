/*
 * Buffer.h
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_BUFFER_H_
#define GFX_BUFFER_BUFFER_H_

#include <glbinding/gl/gl.h>

namespace gfx {
	class Buffer {
		public:
			enum BufferType {
				Array = 1,
				ElementArray
			};

			enum BufferUsage {
				StreamDraw = 1,
				StreamRead,
				StreamCopy,

				StaticDraw,
				StaticRead,
				StaticCopy,

				DynamicDraw,
				DynamicRead,
				DynamicCopy
			};

			enum BufferMapPolicy {
				ReadOnly = 1,
				WriteOnly,
				ReadWrite
			};

		public:
			Buffer(BufferType type, BufferUsage usage = StaticDraw, BufferMapPolicy policy = ReadWrite);
			Buffer() : Buffer(Array, StaticDraw, ReadWrite) {}
			~Buffer();

			void bind(void);

			void unbind(void);
			static void unbind(BufferType type);

			void bufferData(std::size_t size, void *data);
			void replaceData(std::size_t offset, std::size_t size, void *data);
			void reserveData(std::size_t size);

			void *mapBuffer(BufferMapPolicy policy);
			void unmapBuffer();

		private:
			static gl::GLenum bufferTypeGL(BufferType type);
			gl::GLenum bufferTypeGL(void) {
				return bufferTypeGL(this->type);
			}

			static gl::GLenum usageTypeGL(BufferUsage usage);
			gl::GLenum usageTypeGL(void) {
				return usageTypeGL(this->usage);
			}

			static gl::GLenum bufferMapPolicy(BufferMapPolicy policy);

		private:
			gl::GLuint buffer;

			BufferType type;
			BufferUsage usage;
			BufferMapPolicy policy;
	};
} /* namespace gfx */

#endif /* GFX_BUFFER_BUFFER_H_ */
