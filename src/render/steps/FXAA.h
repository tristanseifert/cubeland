/**
 * Implements FXAA, or full-screen antialiasing via a shader.
 *
 * This is basically edge detection on steroids, looks good enough with a very moderate performance
 * overhead.
 */
#ifndef RENDER_STEPS_FXAA_H
#define RENDER_STEPS_FXAA_H

#include <memory>

#include "../RenderStep.h"

// forward-declare a few classes
namespace gfx {
    class ShaderProgram;
    class FrameBuffer;
    class Texture2D;
    class Buffer;
    class VertexArray;
}

namespace render {
class WorldRenderer;

class FXAA : public RenderStep {
    friend class WorldRenderer;

    public:
        FXAA();
        ~FXAA();

        void startOfFrame(void) { }

        void preRender(WorldRenderer *);
        void render(WorldRenderer *);
        void postRender(WorldRenderer *);

        const bool requiresBoundGBuffer() { return false; }
        const bool requiresBoundHDRBuffer() { return false; }

        void reshape(int w, int h);

    protected:
        std::shared_ptr<gfx::FrameBuffer> getFXAABuffer() const {
            return this->inFBO;
        }

    private:
        // gamma component
        float gamma = 2.2f;

        // subpixel aliasing
        float fxaaSubpixelAliasing = 0.74f;
        float fxaaEdgeThreshold = 0.166f;
        float fxaaEdgeThresholdMin = 0.0833f;
        float fxaaEdgeSharpness = 8.0f;

    private:
        std::shared_ptr<gfx::ShaderProgram> program = nullptr;

        std::shared_ptr<gfx::FrameBuffer> inFBO = nullptr;
        std::shared_ptr<gfx::Texture2D> inColor = nullptr;

    // VBO to render a full-screen quad
    private:
        std::shared_ptr<gfx::VertexArray> quadVAO = nullptr;
        std::shared_ptr<gfx::Buffer> quadVBO = nullptr;
};
}

#endif
