#include "Shader.h"

#include "io/Format.h"
#include <Logging.h>

#include <string>
#include <sstream>
#include <cstring>
#include <stdexcept>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gl;
using namespace gfx;

/**
 * Determines the type of the shader from the source code.
 *
 * The first line of the shader should contain either "VERTEX", "FRAGMENT" or
 * "GEOMETRY" in a comment.
 */
Shader::ShaderType Shader::typeFromSource(const std::string &source) {
    // determine the type of shader by analysing the first line
    std::stringstream stream(source);

    std::string source_line1;
    getline(stream, source_line1);

    // allocate a GL shader
    if(source_line1.find("VERTEX") != std::string::npos) {
        return Shader::Vertex;
    } else if(source_line1.find("FRAGMENT") != std::string::npos) {
        return Shader::Fragment;
    } else if(source_line1.find("GEOMETRY") != std::string::npos) {
        return Shader::Geometry;
    }

    // a catch-all if the file sucks
    return Shader::Unknown;
}

/**
 * Initialises a shader with the given source, but waits to compile it.
 */
Shader::Shader(ShaderType type, const std::string &source) {
    // allocate the shader object
    GLenum shaderType = GL_VERTEX_SHADER;

    switch(type) {
        case Vertex:
            shaderType = GL_VERTEX_SHADER;
            break;

        case Fragment:
            shaderType = GL_FRAGMENT_SHADER;
            break;

        case Geometry:
            shaderType = GL_GEOMETRY_SHADER;
            break;

        default:
            Logging::error("Unknown shader type '{}'", type);
            throw std::runtime_error("Unknown shader type");
    }

    this->shader = glCreateShader(shaderType);

    // save the source
    this->type = type;
    this->source = source;
}

/**
 * Releases the allocated resources.
 */
Shader::~Shader() {
    glDeleteShader(this->shader);
}

/**
 * Attempts to compile the shader with the source specified previously. If an
 * error occurs during compilation, an exception is thrown.
 */
void Shader::compile(void) {
    // attempt to compile
    const char *source = this->source.c_str();
    glShaderSource(this->shader, 1, &source, NULL);
    glCompileShader(this->shader);

    // check for error
    constexpr static size_t kErrorLogLen = 1024;
    char errorLog[kErrorLogLen];
    memset(&errorLog, 0, kErrorLogLen);

    GLint success;
    glGetShaderiv(this->shader, GL_COMPILE_STATUS, &success);

    if(!success) {
        glGetShaderInfoLog(this->shader, kErrorLogLen, NULL, errorLog);

        Logging::error("Shader failed to compile:\n{}", errorLog);
        throw std::runtime_error(f("Failed to compile shader {}", errorLog));
    }

    // success
    shaderCompiled = true;
}

/**
 * Attaches the shader to the specified program/
 */
void Shader::attachToProgram(gl::GLuint program) {
    glAttachShader(program, this->shader);
}
