/*
 * ShaderProgram.cpp
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#include "ShaderProgram.h"

#include "io/Format.h"

#include <Logging.h>

#include <string>
#include <sstream>
#include <cstring>
#include <iostream>
#include <fstream>

#include <unistd.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(shaders);

using namespace gl;
using namespace gfx;

/**
 * Creates a rendering shader that loads the vertex and fragment code from the shader resource
 * catalog at the given path.
 */
ShaderProgram::ShaderProgram(const std::string &vertPath, const std::string &fragPath) {
    // create gl program object
    this->program = glCreateProgram();

    // load the vertex and fragment shader code
    auto fs = cmrc::shaders::get_filesystem();

    auto vertex = fs.open(vertPath);
    const auto vertSource = std::string(vertex.begin(), vertex.end());

    auto fragment = fs.open(fragPath);
    const auto fragSource = std::string(fragment.begin(), fragment.end());

    // add shaders
    this->addShaderSource(vertSource, Shader::ShaderType::Vertex);
    this->addShaderSource(fragSource, Shader::ShaderType::Fragment);
}

/**
 * Initialises the shader program.
 */
ShaderProgram::ShaderProgram() {
    this->program = glCreateProgram();
}

/**
 * Deallocates the shader program, and associated resources.
 */
ShaderProgram::~ShaderProgram() {
    glDeleteProgram(this->program);
}

/**
 * Adds a shader to this program from the given string.
 */
void ShaderProgram::addShaderSource(const std::string &source) {
    this->addShaderSource(source, Shader::typeFromSource(source));
}

/*
 * Adds a shader program.
 */
void ShaderProgram::addShaderSource(const std::string &source, const Shader::ShaderType type) {
    auto shader = std::make_shared<Shader>(type, source);
    this->addShader(shader);
}

/**
 * Adds a shader to this program.
 */
void ShaderProgram::addShader(std::shared_ptr<Shader> shader) {
    this->shaders.push_back(shader);
}

/**
 * Attempts to link all shaders together into a single program. If an error
 * occurs, an exception is thrown.
 */
void ShaderProgram::link() {
    // attach all shaders, and compile them, if needed.
    for (auto& shader : this->shaders) {
        // is this shader not yet compiled?
        if(shader->isCompiled() == false) {
            shader->compile();
        }

        // attach to our program
        shader->attachToProgram(this->program);
    }

    // link the program
    glLinkProgram(this->program);

    // check for errors
    constexpr static size_t kErrorLogSz = 1024;
    char errorLog[kErrorLogSz];
    memset(&errorLog, 0, kErrorLogSz);

    GLint success;
    glGetProgramiv(this->program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(this->program, kErrorLogSz, NULL, errorLog);

        Logging::error("Failed to link shader: {}", errorLog);
      	throw std::runtime_error(f("Failed to link shader: {}", errorLog));
    }
}

/**
 * Binds the program to the current context, thus allowing rendering with it.
 */
void ShaderProgram::bind() {
    glUseProgram(this->program);
}

/**
 * Finds the location of a certain attribute.
 */
GLint ShaderProgram::getAttribLocation(const std::string &name) {
    auto loc = glGetAttribLocation(this->program, name.c_str());
#ifndef NDEBUG
    if(loc == -1) {
        Logging::error("Failed to find attribute '{}' on {}", name, (void *) this);
    }
#endif
    return loc;
}

/**
 * Finds the location of a certain uniform.
 */
GLint ShaderProgram::getUniformLocation(const std::string &name) {
    auto loc = glGetUniformLocation(this->program, name.c_str());
    if(loc == -1) {
        Logging::error("Failed to find uniform '{}' on {}", name, (void *) this);
    }
    return loc;
}

/**
 * Binds a varying output variable to the specified colour attachment of the
 * output buffer.
 */
void ShaderProgram::setFragDataLocation(const std::string &name, GLuint loc) {
    glBindFragDataLocation(this->program, loc, name.c_str());
}

/**
 * Sets a uniform's value to the specified single integer.
 */
void ShaderProgram::setUniform1i(const std::string &name, GLint i1) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniform1i(loc, i1);
}

/**
 * Sends a single float value to the specified uniform.
 */
void ShaderProgram::setUniform1f(const std::string &name, gl::GLfloat f1) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniform1f(loc, f1);
}

/**
 * Sends a two-component vector to the specified uniform.
 */
void ShaderProgram::setUniformVec(const std::string &name, glm::vec2 vec) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniform2f(loc, vec.x, vec.y);
}

/**
 * Sends a three-component vector to the specified uniform.
 */
void ShaderProgram::setUniformVec(const std::string &name, glm::vec3 vec) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniform3f(loc, vec.x, vec.y, vec.z);
}

/**
 * Sends a four-component vector to the specified uniform.
 */
void ShaderProgram::setUniformVec(const std::string &name, glm::vec4 vec) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniform4f(loc, vec.x, vec.y, vec.z, vec.w);
}

/**
 * Sends a 3x3 matrix to the specified uniform.
 */
void ShaderProgram::setUniformMatrix(const std::string &name, const glm::mat3 &matrix) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
}

/**
 * Sends a 4x4 matrix to the specified uniform.
 */
void ShaderProgram::setUniformMatrix(const std::string &name, const glm::mat4 &matrix) {
    GLint loc = this->getUniformLocation(name);
    if(loc == -1) return;
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
}
