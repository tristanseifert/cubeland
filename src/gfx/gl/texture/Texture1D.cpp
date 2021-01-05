#include "Texture1D.h"
#include "TextureDumper.h"

#include "io/Format.h"

#include <Logging.h>

#include <stdexcept>

#include <SOIL/SOIL.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <glm/gtc/type_ptr.hpp>

using namespace gl;
using namespace gfx;

/**
 * Allocates a texture object.
 */
Texture1D::Texture1D(int unit, bool bind) {
    this->height = 1;

    // allocate a texture
    glGenTextures(1, &this->texture);
    this->unit = unit;

    // bind and configure
    if(bind == true) {
        this->bind();

        this->wrapS = MirroredRepeat;
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, (GLint) GL_MIRRORED_REPEAT);
        this->wrapT = MirroredRepeat;
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, (GLint) GL_MIRRORED_REPEAT);

        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    }

    // connect to dumper
    TextureDumper::sharedDumper()->registerTexture(this);
}

/**
 * Deallocates the texture.
 */
Texture1D::~Texture1D() {
    glDeleteTextures(1, &this->texture);

    // remove from dumper
    TextureDumper::sharedDumper()->removeTexture(this);
}

/**
 * Binds the texture on the specified texture unit.
 */
void Texture1D::bind(void) {
    // activate texture unit
    glActiveTexture(GL_TEXTURE0 + ((unsigned int) this->unit));

    // bind
    glBindTexture(GL_TEXTURE_1D, this->texture);
}

/**
 * Unbinds the texture.
 */
void Texture1D::unbind(void) {
    // select the correct texture unit
    // glActiveTexture(GL_TEXTURE0);

    // unbind
    glBindTexture(GL_TEXTURE_1D, 0);
}

/**
 * Writes the 1D texture out to disk.
 */
void Texture1D::dump(const std::string &base) {
    // save colour textures as TGA
    if(this->format == DepthGeneric || this->format == Depth24Stencil8) {
        // figure out the filename
        const auto name = f("{}tex1D_{}.raw", base, this->debugName);
        std::fstream file(name, std::ios::out | std::ios::binary | std::ios::trunc);

        if(file.is_open()) {
            Logging::info("Dumping to {}: width = {}", name, this->width);

            // bind texture and allocate a buffer, read texture and write to file
            size_t bufferSz = this->width * this->height * 4;
            char *buffer = new char[bufferSz];

            this->bind();
            glGetTexImage(GL_TEXTURE_1D, 0, GL_DEPTH_COMPONENT, GL_INT, buffer);

            file.write(buffer, bufferSz);

            // close file
            delete[] buffer;
            file.close();
        } else {
            Logging::warn("Failed to dump to {}: {}", name, errno);
        }
    } else {
        // store TGA
        const auto name = f("{}tex1D_{}.tga", base, this->debugName);
        Logging::info("Dumping to {}: width = {}", name, this->width);

        // bind texture and allocate a buffer, read texture and write to file
        size_t bufferSz = this->width * this->height * 4;
        char *buffer = new char[bufferSz];

        this->bind();
        glGetTexImage(GL_TEXTURE_1D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

        SOIL_save_image(name.c_str(), SOIL_SAVE_TYPE_TGA, width, height, 4,
                                        (const unsigned char *) buffer);

        delete[] buffer;
    }
}

/**
 * Allocates texture memory of the given width and height, but does not fill it with anything.
 */
void Texture1D::allocateBlank(const size_t width, const TextureFormat format) {
    this->bind();

    // copy format
    this->format = format;
    this->width = width;

    // Get the colour format
    GLenum colourFormat = GL_RGB;
    GLenum dataType = GL_FLOAT;

    if(format == RGBA || format == RGBA8 || format == RGBA16F || format == RGBA32F) {
            colourFormat = GL_RGBA;
    } else if(format == DepthGeneric || format == Depth24Stencil8) {
            colourFormat = GL_DEPTH_COMPONENT;
    } else if(format == RG8 || format == RG16F || format == RG32F) {
            colourFormat = GL_RG;
    } else if(format == RED8 || format == RED16F || format == RED32F) {
            colourFormat = GL_RED;
    }

    if(format == RGB || format == RGBA || format == RGB8 || format == RGBA8 || format == RED8 ||
            format == RG8) {
            dataType = GL_UNSIGNED_BYTE;
    }

    if(format == RG16F || format == RGBA16F || format == RGB16F || format == RED16F ||
       format == DepthGeneric) {
            dataType = GL_FLOAT;
    }

    // allocate memory
    GLint type = (GLint) this->glFormat();

    glTexImage1D(GL_TEXTURE_1D, 0, type, (GLsizei) width, 0, colourFormat, dataType, nullptr);

    // use nearest neighbour interpolation
    if(!this->usesLinearFiltering) {
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, (GLint) GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, (GLint) GL_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    }


    // unbind texture
    Texture1D::unbind();
}

/**
 * Loads a subset of data to the texture.
 */
void Texture1D::bufferSubData(const size_t width, const size_t xOff, const TextureFormat format, const void *data) {
    this->bind();

    // copy format
    this->format = format;

    // Get the colour format
    GLenum colourFormat = GL_RGB;
    GLenum dataType = GL_FLOAT;

    if(format == RGBA || format == RGBA8 || format == RGBA16F || format == RGBA32F) {
            colourFormat = GL_RGBA;
    } else if(format == DepthGeneric || format == Depth24Stencil8) {
            colourFormat = GL_DEPTH_COMPONENT;
    } else if(format == RG8) {
            colourFormat = GL_RG;
    } else if(format == RED8 || format == RED16F || format == RED32F) {
            colourFormat = GL_RED;
    }

    if(format == RGB || format == RGBA || format == RGB8 || format == RGBA8 || format == RED8 ||
            format == RG8) {
            dataType = GL_UNSIGNED_BYTE;
    }

    glTexSubImage1D(GL_TEXTURE_1D, 0, (GLint) xOff, (GLsizei) width, colourFormat, dataType, data);

    // unbind texture
    Texture1D::unbind();
}

/**
 * Loads the texture's image from the given path. The texture is loaded as an RGB image.
 */
void Texture1D::loadFromImage(const std::string &path, bool sRGB) {
    this->bind();

    // store path
    this->loadPaths.clear();
    this->loadPaths.push_back(path);

    // load image
    int width, height;
    GLenum format, internalFormat;

    void *data = loadImageData(path, &width, &height, &format);
    if(data == NULL) {
        Logging::error("Failed to load texture {}", path);
        return;
    }

    // load as sRGB if needed
    if(sRGB) {
        internalFormat = (format == GL_RGB) ? GL_SRGB : GL_SRGB_ALPHA;
    } else {
        internalFormat = format;
    }

    // load the data to a texture and create mipmaps
    glTexImage1D(GL_TEXTURE_1D, 0, (GLint) internalFormat, width, 0, format, GL_UNSIGNED_BYTE, data);

    releaseImageData(data);

    this->format = RGB;
    this->width = (unsigned int) width;

    // unbind texture
    Texture1D::unbind();
}

/**
 * Sets whether the texture interpolates linearly or not.
 */
void Texture1D::setUsesLinearFiltering(bool enabled) {
    // bind texture
    bind();

    this->usesLinearFiltering = enabled;

    // set the filtering state
    if(enabled) {
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, (GLint) GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, (GLint) GL_NEAREST);
    }

    // unbind
    Texture1D::unbind();
}

/**
 * Sets the wrapping mode of the texture.
 */
void Texture1D::setWrapMode(WrapMode s, WrapMode t) {
    // store
    this->wrapS = s;
    this->wrapT = t;

    // send to texture
    bind();

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, (GLint) glWrapMode(s));
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, (GLint) glWrapMode(t));

    // unbind
    Texture1D::unbind();
}

/**
 * Sets the colour of the border of the texture.
 */
void Texture1D::setBorderColor(const glm::vec4 &border) {
    this->borderColour = border;

    this->bind();
    glTexParameterfv(GL_TEXTURE_1D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border));

    // unbind
    Texture1D::unbind();
}
