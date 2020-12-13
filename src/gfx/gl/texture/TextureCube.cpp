/*
 * TextureCube.cpp
 *
 *  Created on: Aug 24, 2015
 *      Author: tristan
 */

#include "TextureCube.h"

#include <Logging.h>

#include <stdexcept>

#include <SOIL/SOIL.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include "TextureDumper.h"

using namespace gl;
using namespace gfx;

/**
 * Allocates a texture object.
 */
TextureCube::TextureCube(int unit) {
	// allocate a texture
	glGenTextures(1, &this->texture);
	this->unit = unit;

	// bind and configure
	bind();

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, (GLint) GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, (GLint) GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);

	// connect to dumper
	TextureDumper::sharedDumper()->registerTexture(this);
}

/**
 * Deallocates the texture.
 */
TextureCube::~TextureCube() {
	glDeleteTextures(1, &this->texture);

	// remove from dumper
	TextureDumper::sharedDumper()->removeTexture(this);
}

/**
 * Binds the texture on the specified texture unit.
 */
void TextureCube::bind(void) {
	// activate texture unit
	glActiveTexture(GL_TEXTURE0 + ((unsigned int) this->unit));

	// bind
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->texture);
}

/**
 * Unbinds the texture.
 */
void TextureCube::unbind(void) {
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

/**
 * Writes the 2D texture out to disk.
 */
void TextureCube::dump(const std::string &base) {
    // not implemented
    Logging::debug("TextureCube::dump() is unimplemented (this = {}, base = {})", (void *) this, base);
}

/**
 * Allocates texture memory of the given width and height, but does not fill it
 * with anything.
 *
 * @note Each of the six faces is allocated the same format texture.
 */
void TextureCube::allocateBlank(unsigned int width, unsigned int height, TextureFormat format) {
	this->bind();

	// copy format
	this->format = format;

	this->width = width;
	this->height = height;

	// Get the colour format
	GLenum colourFormat = GL_RGB;
	GLenum dataType = GL_FLOAT;

	if(format == RGBA || format == RGBA8 || format == RGBA16F || format == RGBA32F) {
		colourFormat = GL_RGBA;
	} else if(format == Depth24Stencil8) {
		colourFormat = GL_DEPTH_COMPONENT;
	}

	if(format == RGB || format == RGBA || format == RGB8 || format == RGBA8) {
		dataType = GL_UNSIGNED_BYTE;
	}

	// allocate memory
	GLint type = (GLint) glFormat();

	glTexImage2D(GL_TEXTURE_CUBE_MAP, 0, type, (GLsizei) width, (GLsizei) height, 0, colourFormat,
                 dataType, nullptr);

	// unbind texture
	TextureCube::unbind();
}

/**
 * Loads a subpart of a cubemap's face to the texture.
 */
void TextureCube::bufferSubData(unsigned int width, unsigned int height, unsigned int xOff,
                                unsigned int yOff, TextureFormat format, void *data) {
    throw std::runtime_error("TextureCube::bufferSubData is not implemented");
}

/**
 * Loads a cubemap from the different images specified.
 *
 * @note The order of images is +X, -X, +Y, -Y, +Z, -Z.
 */
void TextureCube::loadFromImages(const std::vector<std::string> &paths, bool sRGB) {
	// ensure we have six images
    XASSERT(paths.size() == 6, "Cubemaps must load all six textures at once");
	this->bind();

	// store path
	this->loadPaths.clear();

	// go through all images and individually load them
	GLenum format, internalFormat, target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

	for(auto & path : paths) {
		// store path
		this->loadPaths.push_back(path);
        
        int width, height;
        void *data;

        // load image
        data = loadImageData(path, &width, &height, &format);
        if(!data) {
            Logging::error("Failed to load cubemap texture {}", path);
            continue;
        }

        // load as sRGB if needed
        if(sRGB) {
            internalFormat = (format == GL_RGB) ? GL_SRGB : GL_SRGB_ALPHA;
        } else {
            internalFormat = format;
        }

        // shove it into the texture
        glTexImage2D(target, 0, (GLint) internalFormat, width, height, 0,
                     format, GL_UNSIGNED_BYTE, data);

        // go to the next texture and clean up
        releaseImageData(data);

        // save the width/height
        this->width = (unsigned int) width;
        this->height = (unsigned int) height;

	    target = (GLenum) (((int) target) + 1);
	}

	// set up some other state
	if(format == GL_RGB) {
		this->format = Texture::RGB;
	} else {
		this->format = Texture::RGBA;
	}

	// unbind
	TextureCube::unbind();
}
