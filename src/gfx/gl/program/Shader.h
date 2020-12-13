/*
 * Shader.h
 *
 * A single shader program, such as a vertex or fragment shader, which will be
 * compiled from source, then linked into a Program.
 *
 *  Created on: Aug 21, 2015
 *      Author: tristan
 */

#ifndef GFX_PROGRAM_SHADER_H_
#define GFX_PROGRAM_SHADER_H_

#include <string>

#include <glbinding/gl/gl.h>

namespace gfx {
class Shader {
    public:
        enum ShaderType {
            Vertex,
            Fragment,
            Geometry,

            Unknown = -1
        };

    public:
        Shader(ShaderType type, const std::string &source);
        ~Shader();

        void compile(void);

        bool isCompiled() {
            return shaderCompiled;
        }

    public:
        void attachToProgram(gl::GLuint program);

        static ShaderType typeFromSource(const std::string &source);

    private:
        std::string source;

        ShaderType type;
        bool shaderCompiled = false;
        gl::GLuint shader;
};
} /* namespace gfx */

#endif /* GFX_PROGRAM_SHADER_H_ */
