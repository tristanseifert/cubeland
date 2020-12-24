#ifndef RENDER_STEPS_SSAO_H
#define RENDER_STEPS_SSAO_H

#include "../RenderStep.h"

#include <memory>
#include <vector>

#include <glm/vec3.hpp>

namespace gfx {
class Texture2D;
class ShaderProgram;
class Buffer;
class FrameBuffer;
class VertexArray;
}

namespace render {
class Lighting;

class SSAO: public RenderStep {
    friend class WorldRenderer;
    friend class Lighting;

    public:
        SSAO();
        ~SSAO();

        void startOfFrame(void);
        void preRender(WorldRenderer *);
        void postRender(WorldRenderer *) {};

        void render(WorldRenderer *);

        const bool requiresBoundGBuffer() { return false; }
        const bool requiresBoundHDRBuffer() { return false; }

        void reshape(int w, int h);

        void setDepthTex(std::shared_ptr<gfx::Texture2D> depth) {
            this->gDepth = depth;
        }
        void setNormalTex(std::shared_ptr<gfx::Texture2D> normal) {
            this->gNormal = normal;
        }

    private:
        void initQuadBuf();
        void initOcclusionBuf();
        void initOcclusionBlurBuf();

        void initNoiseTex();
        void generateKernel(size_t size = 64);

        void loadOcclusionShader();
        void loadOcclusionBlurShader();
        void sendKernel(gfx::ShaderProgram *program);

        void drawDebugWindow();

    private:
        std::shared_ptr<gfx::Texture2D> gNormal = nullptr;
        std::shared_ptr<gfx::Texture2D> gDepth = nullptr;

        gfx::VertexArray *vao = nullptr;
        gfx::Buffer *vbo = nullptr;

        /// 16-bit single component texture holding the occlusion value
        gfx::Texture2D *occlusionTex = nullptr;
        /// 16-bit single component texture holding blurred occlusion value
        gfx::Texture2D *occlusionBlurTex = nullptr;

        /// Framebuffer bound for the occlusion finding shader
        gfx::FrameBuffer *occlusionFb = nullptr;
        /// Framebuffer for blurring occlusion buffer
        gfx::FrameBuffer *occlusionBlurFb = nullptr;

        /// SSAO kernel
        std::vector<glm::vec3> kernel;
        /// we need a small noise texture to allow us to blur the SSAO output nicely
        gfx::Texture2D *noiseTex = nullptr;

        /// shader for calculating the occlusion value
        gfx::ShaderProgram *occlusionShader = nullptr;

        /// shader for performing the occlusion blur
        gfx::ShaderProgram *occlusionBlurShader = nullptr;

    private:
        // when set, the shader receives new SSAO params
        bool needsParamUpdate = true;
        // when set, the kernel is updated to the shader
        bool needsKernelUpdate = true;

        bool enabled = false;

        int ssaoKernelSize = 64;
        float ssaoRadius = 0.5;
        float ssaoBias = 0.025;
};
}

#endif
