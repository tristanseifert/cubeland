/**
 * Draws a quaint little plasma effect into a texture.
 */
#ifndef GUI_TITLE_PLASMARENDERER_H
#define GUI_TITLE_PLASMARENDERER_H

#include <glm/vec2.hpp>

namespace gfx {
class Buffer;
class FrameBuffer;
class ShaderProgram;
class Texture2D;
class VertexArray;
}

namespace gui::title {
class PlasmaRenderer {
    public:
        PlasmaRenderer(const glm::ivec2 &size, const size_t blurPasses = 0);
        ~PlasmaRenderer();

        /// Resizes the output buffer
        void resize(const glm::ivec2 &size);

        /// Draws the plasma image to the output texture
        void draw(const double time);
        /// Gets a reference to the output texture
        gfx::Texture2D *getOutput() const {
            return this->outTex;
        }

    private:
        // plasma drawing shader
        gfx::ShaderProgram *program = nullptr;

        // number of blur passes
        size_t blurPasses = 0;
        // blurring shader
        gfx::ShaderProgram *blurProgram = nullptr;
        // blur intermediate framebuffer
        gfx::FrameBuffer *blurFb = nullptr;
        // blur intermediate texture
        gfx::Texture2D *blurTex = nullptr;

        // viewport size
        glm::ivec2 viewport;
        // render destination
        gfx::FrameBuffer *fb = nullptr;
        // texture backing the framebuffer
        gfx::Texture2D *outTex = nullptr;

        // buffer holding vertices for the full screen quad
        gfx::Buffer *vertices = nullptr;
        // vertex array defining vertices
        gfx::VertexArray *vao = nullptr;
};
}

#endif
