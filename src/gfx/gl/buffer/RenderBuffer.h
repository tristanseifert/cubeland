/*
 * RenderBuffer.h
 *
 * Encapsulates an OpenGL render buffer, for use as a write-only render target
 * in a framebuffer.
 *
 *  Created on: Aug 22, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_RENDERBUFFER_H_
#define GFX_BUFFER_RENDERBUFFER_H_

#include <glbinding/gl/gl.h>

namespace gfx {
	class RenderBuffer {
		public:
			RenderBuffer(unsigned int width, unsigned int height);
			virtual ~RenderBuffer();

			void bind(void);
			static void unbind(void);

			void allocateDepth();

			// This should not be accessed from external code.
			gl::GLuint rbo;

		private:
			unsigned int width, height;
	};
} /* namespace gfx */

#endif /* GFX_BUFFER_RENDERBUFFER_H_ */
