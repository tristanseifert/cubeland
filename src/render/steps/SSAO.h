#ifndef RENDER_STEPS_SSAO_H
#define RENDER_STEPS_SSAO_H

#include "../RenderStep.h"

#include <memory>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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
        void postRender(WorldRenderer *);

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

        void initNoiseTex();
        void generateKernel(size_t size = 64);

        void loadOcclusionShader();
        void sendKernel(gfx::ShaderProgram *program);

        void drawDebugWindow();
        void drawSsaoPreview();

    private:
        std::shared_ptr<gfx::Texture2D> gNormal = nullptr;
        std::shared_ptr<gfx::Texture2D> gDepth = nullptr;

        gfx::VertexArray *vao = nullptr;
        gfx::Buffer *vbo = nullptr;

        /// size of the occlusion buffers
        glm::vec2 occlusionSize;
        /// 16-bit single component texture holding the occlusion value
        gfx::Texture2D *occlusionTex = nullptr;
        /// Framebuffer bound for the occlusion finding shader
        gfx::FrameBuffer *occlusionFb = nullptr;


        /// SSAO kernel
        std::vector<glm::vec3> kernel;
        /// we need a small noise texture to allow us to blur the SSAO output nicely
        gfx::Texture2D *noiseTex = nullptr;

        /// shader for calculating the occlusion value
        gfx::ShaderProgram *occlusionShader = nullptr;


    private:
        // when set, the shader receives new SSAO params
        bool needsParamUpdate = true;
        // when set, the kernel is updated to the shader
        bool needsKernelUpdate = true;

        bool enabled = true;

        int ssaoKernelSize = 8;
        float ssaoRadius = 0.5;
        float ssaoBias = 0.025;

        bool showSsaoPreview = false;
        glm::vec4 ssaoPreviewTint = glm::vec4(1,0,0,1);
        int previewTextureIdx = 0;
};
}

#endif
