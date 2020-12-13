#ifndef RENDER_STEPS_LIGHTING_H
#define RENDER_STEPS_LIGHTING_H

#include "../RenderStep.h"

#include <memory>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace gfx {
class DirectionalLight;
class SpotLight;

class ShaderProgram;
class FrameBuffer;
class Buffer;
class VertexArray;
class Texture2D;
class TextureCube;

class RenderProgram;

namespace lights {
class AbstractLight;
}
}

namespace render {
class WorldRenderer;
class SceneRenderer;

class Lighting : public RenderStep {
    friend class WorldRenderer;

    public:
        Lighting();
        ~Lighting();

        void startOfFrame();
        void preRender(WorldRenderer *);
        void render(WorldRenderer *);
        void postRender(WorldRenderer *);

        const bool requiresBoundGBuffer() { return false; }
        const bool requiresBoundHDRBuffer() { return true; }

        void reshape(int w, int h);

        void addLight(std::shared_ptr<gfx::lights::AbstractLight> light);
        int removeLight(std::shared_ptr<gfx::lights::AbstractLight> light);

        void bindGBuffer(void);
        void unbindGBuffer(void);

    private:
        void setUpRenderBuffer();

        void setUpTestLights(void);
        void sendLightsToShader(void);

        void setUpSkybox(void);
        void renderSkybox(void);

        void setUpShadowing(void);

        void renderShadowMap(WorldRenderer *);

        void setSceneRenderer(std::shared_ptr<SceneRenderer> s) {
            this->shadowSceneRenderer = s;
        }

    private:
        std::shared_ptr<gfx::ShaderProgram> program = nullptr;

        std::shared_ptr<gfx::FrameBuffer> fbo = nullptr;

        std::shared_ptr<gfx::Texture2D> gDepth = nullptr;
        std::shared_ptr<gfx::Texture2D> gNormal = nullptr;
        std::shared_ptr<gfx::Texture2D> gDiffuse = nullptr;
        std::shared_ptr<gfx::Texture2D> gMatProps = nullptr;

        std::shared_ptr<gfx::VertexArray> vao = nullptr;
        std::shared_ptr<gfx::Buffer> vbo = nullptr;

    private:
        // lights
        std::vector<std::shared_ptr<gfx::lights::AbstractLight>> lights;

        std::shared_ptr<gfx::SpotLight> spot = nullptr;
        std::shared_ptr<gfx::DirectionalLight> sun = nullptr;

    private:
        // the skybox is drawn after the lighting pass
        std::shared_ptr<gfx::VertexArray> vaoSkybox = nullptr;
        std::shared_ptr<gfx::Buffer> vboSkybox = nullptr;

        std::shared_ptr<gfx::ShaderProgram> skyboxProgram = nullptr;

        std::shared_ptr<gfx::TextureCube> skyboxTexture = nullptr;

        bool skyboxEnabled = false;

    private:
        // Fog density and color
        float fogDensity = 0.03f;
        glm::vec3 fogColor = glm::vec3(0.53, 0.8, 0.98);
        // This is subtracted from the distance to push the fog back
        float fogOffset = 25.f;

    private:
        glm::mat4 shadowViewMatrix;

        // Scene renderer to use for rendering shadows
        std::shared_ptr<SceneRenderer> shadowSceneRenderer = nullptr;

        // Shadow texture dimensions
        unsigned int shadowW = 1024, shadowH = 1024;
        // Shadow model rendering program
        std::shared_ptr<gfx::RenderProgram> shadowRenderProgram = nullptr;
        // Framebuffer object with shadow depth texture
        std::shared_ptr<gfx::FrameBuffer> shadowFbo = nullptr;
        // Shadow depth texture
        std::shared_ptr<gfx::Texture2D> shadowTex = nullptr;
        // Shadow color texture (debugging)
        std::shared_ptr<gfx::Texture2D> shadowColorTex = nullptr;

        // constant to multiply directional light by
        float shadowDirectionCoefficient = 2.f;

        // XXX: testing
        double time = 0;

    private:
        void drawDebugWindow();
        void drawLightsTable();

        // when set, the render debug window is shown
        bool showDebugWindow = true;
};
}

#endif
