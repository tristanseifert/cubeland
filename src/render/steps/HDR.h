#ifndef RENDER_STEPS_HDR_H
#define RENDER_STEPS_HDR_H

#include "../RenderStep.h"

#include <memory>

namespace gfx {
class Texture2D;
class ShaderProgram;
class Buffer;
class FrameBuffer;
class VertexArray;
}

namespace render {
class HDR: public RenderStep {
    friend class Lighting;

    public:
        HDR();
        ~HDR();

        void startOfFrame(void);

        void preRender(WorldRenderer *);
        void render(WorldRenderer *);
        void postRender(WorldRenderer *);

        const bool requiresBoundGBuffer() { return false; }
        const bool requiresBoundHDRBuffer() { return false; }

        void reshape(int w, int h);

        void bindHDRBuffer(void);
        void unbindHDRBuffer(void);

        void setDepthBuffer(std::shared_ptr<gfx::Texture2D>);
        void setOutputFBO(std::shared_ptr<gfx::FrameBuffer>, bool attach = true);

    private:
        void setUpInputBuffers(void);
        void setUpHDRLumaBuffers(void);
        void setUpBloom(void);
        void setUpTonemap(void);

        void renderExtractBright(void);
        void renderBlurBright(void);
        void renderPerformTonemapping(void);

    private:
        /**
         * This is the framebuffer into which we render the tonemapped output data. Before
         * rendering, we attach the luminance output texture as color attachment 1. Color
         * attachment 0 is passed on to later steps of the pipeline, namely FXAA.
         */
        std::shared_ptr<gfx::FrameBuffer> outFBO = nullptr;

    // stuff to acquire the HDR render buffer
    private:
        std::shared_ptr<gfx::ShaderProgram> inHdrProgram = nullptr;
        std::shared_ptr<gfx::FrameBuffer> inFBO = nullptr;
        std::shared_ptr<gfx::Texture2D> inColour = nullptr, inDepth = nullptr;

        std::shared_ptr<gfx::FrameBuffer> hdrLumaFBO = nullptr;
        std::shared_ptr<gfx::Texture2D> sceneLuma = nullptr;

    // stuff to render bloom
    private:
        std::shared_ptr<gfx::ShaderProgram> bloomBlurProgram = nullptr;
        std::shared_ptr<gfx::FrameBuffer> inFBOBloom1 = nullptr, inFBOBloom2 = nullptr;
        std::shared_ptr<gfx::Texture2D> inBloom1 = nullptr, inBloom2 = nullptr;

        // whether blooming is enabled or not
        bool bloomEnabled = false;
        // number of passes to perform for blurring (must be a multiple of 2)
        int numBlurPasses = 3;
        // size of the blur kernel to use (5, 9, or 13)
        const int blurSize = 13;
        // Number to divide viewport size with when blurring
        int bloomTexDivisor = 2;

        // when set, the bloom buffers are "dirty" and must be disabled after bloom is turned off
        bool bloomBufferDirty = true;

        // additive blending factor for blooming
        float bloomFactor = 1;

    private:
        // exposure value
        float exposure = 1;
        // current frame's avg luminance
        double frameAvgLuma = 0.f;

        // mask for histo counter
        unsigned int histoFrameWait = 8;
        // histogram counter: when the low 4 bits are all 1, calculate histogram
        unsigned int histoCounter = 0;

        // Histogram calculator lel
        // gfx::HistogramCalculator *lumaHisto;

    private:
        // update the exposure value
        void _updateExposure(void);
        // slowly step the exposure
        void _exposureStep(void);

        // which direction the exposure should change
        enum {
            NONE,
            DOWN,
            UP
        } exposureDirection = NONE;
        // how long exposure change has been ongoing
        double exposureChangeTicks = 0.f;
        // this is multiplied by the delta to vary the speed of the exposure change
        double exposureDeltaMultiplier = 1.f;

    // stuff to perform tonemapping
    private:
        std::shared_ptr<gfx::ShaderProgram> tonemapProgram = nullptr;

        /// white point
        glm::vec3 whitePoint = glm::vec3(1, 1, 1);

    // VBO to render a full-screen quad
    private:
        std::unique_ptr<gfx::VertexArray> quadVAO;
        std::unique_ptr<gfx::Buffer> quadVBO;

    private:
        void drawDebugWindow();

        // when set, the render debug window is shown
        bool showDebugWindow = true;
};
}

#endif
