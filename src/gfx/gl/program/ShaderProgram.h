#ifndef GFX_PROGRAM_SHADERPROGRAM_H_
#define GFX_PROGRAM_SHADERPROGRAM_H_

#include <vector>
#include <string>
#include <memory>

#include <glbinding/gl/gl.h>

#include <glm/glm.hpp>

#include "Shader.h"

namespace gfx {
class ShaderProgram {
    public:
        ShaderProgram();
        ShaderProgram(const std::string &vertPath, const std::string &fragPath);
        ~ShaderProgram();

        void addShaderSource(const std::string &source);
        void addShaderSource(const std::string &source, const Shader::ShaderType type);
        void addShader(std::shared_ptr<Shader> shader);

        void link();

        void bind();

        gl::GLint getAttribLocation(const std::string &name);
        gl::GLint getUniformLocation(const std::string &name);

        void setFragDataLocation(const std::string &name, gl::GLuint loc);

        void setUniform1i(const std::string &name, gl::GLint i1);
        void setUniform1f(const std::string &name, gl::GLfloat f1);

        void setUniformVec(const std::string &name, glm::vec2 vec);
        void setUniformVec(const std::string &name, glm::vec3 vec);
        void setUniformVec(const std::string &name, glm::vec4 vec);

        void setUniformMatrix(const std::string &name, const glm::mat3 &matrix);
        void setUniformMatrix(const std::string &name, const glm::mat4 &matrix);

    private:
        gl::GLuint program;

        std::vector<std::shared_ptr<Shader>> shaders;
    };
} /* namespace gfx */

#endif /* GFX_PROGRAM_SHADERPROGRAM_H_ */
