/*
 * Texture.cpp
 *
 *  Created on: Aug 24, 2015
 *      Author: tristan
 */

#include "Texture.h"

#include <Logging.h>

#include <cassert>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <SOIL/SOIL.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// FourCC for DXT textures
#define FOURCC_DXT1	0x31545844
#define FOURCC_DXT2	0x32545844
#define FOURCC_DXT3	0x33545844
#define FOURCC_DXT4	0x34545844
#define FOURCC_DXT5	0x35545844

using namespace std;
using namespace gl;
using namespace gfx;

/**
 * Allocates a texture object.
 */
Texture::Texture(int unit) {
	// allocate a texture
	glGenTextures(1, &this->texture);
	this->unit = unit;
}

/**
 * Deallocates the texture.
 */
Texture::~Texture() {
	glDeleteTextures(1, &this->texture);
}

/**
 * Loads image data using SOIL.
 *
 * @note width, height and format MUST be specified.
 */
void *Texture::loadImageData(string path, int *width, int *height, GLenum *format) {
	// load image
	int channels;
	unsigned char* image = SOIL_load_image(path.c_str(),
										   width, height, &channels,
										   SOIL_LOAD_AUTO);

	// Determine type (RGB/RGBA)
	*format = GL_RGBA;

	if(channels == 3) {
		*format = GL_RGB;
	}

	return image;
}

/**
 * Releases previously loaded image data.
 */
void Texture::releaseImageData(void *data) {
	SOIL_free_image_data((unsigned char *) data);
}

/**
 * Loads a DXT file, and populates some information based off of it.
 *
 * @note The assumption is made that all DXT1 textures have no alpha component.
 */
int Texture::loadDDSFile(std::string path) {
	// allocate a buffer into which we read the actual images
	assert(this->ddsData == NULL);
	this->ddsData = new std::vector<char>();

    std::vector<char> header;
    header.reserve(124);

	// try to open the file and read it
	std::ifstream in(path, ios::in | ios::binary);
	if(in.is_open() == false) {
		cerr << "Could not open " << path << " for DDS loading" << endl;
		return -1;
	}

    in.seekg(0, ios::beg);

    // read and validate the 4CC code of the DDS file itself
	in.read(&header.front(), 0x04);

    if(strncmp(&header.front(), "DDS ", 4) != 0) {
		cerr << "Invalid DDS file: " << path << endl;
    	return -2;
    }

    // read the header
    in.seekg(0x04);
	in.read(&header.front(), 0x7C);

	// parse some information
    this->height = *(unsigned int*) &(header[8]);
    this->width = *(unsigned int*) &(header[12]);

    unsigned int linearSize = *(unsigned int*) &(header[16]);
    unsigned int fourCC = *(unsigned int*) &(header[80]);

    this->mipMapCount = *(unsigned int*) &(header[24]);

    // figure out the final buffer size, and read the file
    size_t buffSize =  this->mipMapCount > 1 ? linearSize * 2 : linearSize;
    this->ddsData->resize(buffSize);

    in.seekg(0x80);
	in.read(&(this->ddsData->front()), buffSize);
	in.close();

	// determine the internal format we want to use
	switch(fourCC) {
		case FOURCC_DXT1:
			this->loadedFormat = DXT1;
			break;

		case FOURCC_DXT3:
			this->loadedFormat = DXT3;
			break;

		case FOURCC_DXT5:
			this->loadedFormat = DXT5;
			break;

		default:
			cerr << "Unknown FourCC: 0x" << hex << fourCC << dec << endl;
			return -3;
	}

	// we're good
	return 0;
}

/**
 * Releases some data allocated in the process of loading the DDS file.
 */
void Texture::releaseDDSFile(void) {
	// delete buffer
	delete this->ddsData;
	this->ddsData = NULL;
}

/**
 * Determines the OpenGL format for the loaded compressed file type.
 */
GLenum Texture::glTypeForLoadFormat(bool sRGB) {
	GLenum internalFormat;

	if(sRGB) {
		switch(this->loadedFormat) {
			case Texture::DXT1:
				internalFormat = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
				break;

			case Texture::DXT3:
				internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
				break;

			case Texture::DXT5:
				internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
				break;

			default:
				internalFormat = GL_SRGB_ALPHA;
				break;
		}
	} else {
		switch(this->loadedFormat) {
			case Texture::DXT1:
				internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				break;

			case Texture::DXT3:
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				break;

			case Texture::DXT5:
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				break;

			default:
				internalFormat = GL_RGBA;
				break;
		}
	}

	return internalFormat;

}

/**
 * Converts the internal representation of the texture format to the OpenGL
 * equivalent.
 */
GLenum Texture::glFormat(void) {
	switch(this->format) {
		case RED8:
			return GL_R8;
		case RED16F:
			return GL_R16F;
		case RED32F:
			return GL_R32F;

		case RG8:
			return GL_RG8;
		case RG16F:
			return GL_RG16F;
		case RG32F:
			return GL_RG32F;

		case RGB:
			return GL_RGB;
		case RGB8:
			return GL_RGB8;
		case RGB16F:
			return GL_RGB16F;
		case RGB32F:
			return GL_RGB32F;

		case RGBA:
			return GL_RGBA;
		case RGBA8:
			return GL_RGBA8;
		case RGBA16F:
			return GL_RGBA16F;
		case RGBA32F:
			return GL_RGBA32F;

		case DepthGeneric:
			return GL_DEPTH_COMPONENT;
		case Depth24Stencil8:
			return GL_DEPTH24_STENCIL8;

		default:
			return GL_RGBA;
	}
}

/**
 * Converts a wrapping mode to the OpenGL enum value.
 */
GLenum Texture::glWrapMode(WrapMode mode) {
	switch(mode) {
		case Clamp:
			return GL_CLAMP;
		case ClampToBorder:
			return GL_CLAMP_TO_BORDER;
		case ClampToEdge:
			return GL_CLAMP_TO_EDGE;
		case MirroredRepeat:
			return GL_MIRRORED_REPEAT;
		case Repeat:
			return GL_REPEAT;
	}

	// undefined - should not get here
	return (gl::GLenum) 0;
}

/**
 * Sets the colour of the border of the texture.
 */
void Texture::setBorderColour(glm::vec4 border) {
	// store and send
	this->borderColour = border;

	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border));
}
