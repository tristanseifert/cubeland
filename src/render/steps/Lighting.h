#ifndef RENDER_STEPS_LIGHTING_H
#define RENDER_STEPS_LIGHTING_H

#include "../RenderStep.h"

#include <memory>
#include <vector>

#include <glm/vec2.hpp>
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

        void startOfFrame();
        void preRender(WorldRenderer *);
        void render(WorldRenderer *);
        void postRender(WorldRenderer *);

        const bool requiresBoundGBuffer() { return false; }
        const bool requiresBoundHDRBuffer() { return true; }

        void reshape(int w, int h);

        void addLight(std::shared_ptr<gfx::lights::AbstractLight> light);
        void removeLight(std::shared_ptr<gfx::lights::AbstractLight> light);

        void bindGBuffer(void);
        void unbindGBuffer(void);

        void setOcclusionTex(gfx::Texture2D *texture) {
            this->occlusionTex = texture;
        }

    private:
        void setUpRenderBuffer();

        void sendLightsToShader(void);

        void setUpSky();
        void generateSkyNoise();
        void renderSky(WorldRenderer *);
        void updateSunAngle(WorldRenderer *);
        void updateMoonAngle(WorldRenderer *);

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

        // from SSAO
        gfx::Texture2D *occlusionTex = nullptr;

    private:
        // intensity of ambient light
        float ambientIntensity = 0.0125;

        // lights
        std::vector<std::shared_ptr<gfx::lights::AbstractLight>> lights;
        // forces all lights to be sent
        bool sendAllLights = false;

    private:
        // the sun is modelled as a directional light
        std::shared_ptr<gfx::DirectionalLight> sun = nullptr;
        // direction of the sun in the sky
        glm::vec3 sunDirection;
        // default color for the sun. this is used when the sun is in the upper 85% of its cycle
        glm::vec3 sunColorNormal = glm::vec3(1, 1, 1);

        // we also have one for the moon
        std::shared_ptr<gfx::DirectionalLight> moon = nullptr;
        // direction of the moon
        glm::vec3 moonDirection;
        // default color for the moon light
        glm::vec3 moonColorNormal = glm::vec3(0.1, 0.1, 0.133);

    private:
        std::shared_ptr<gfx::ShaderProgram> skyProgram = nullptr;
        std::shared_ptr<gfx::Texture2D> skyNoiseTex = nullptr;

        bool skyEnabled = true;

        // when set, the sky data is re-sent to the shader
        bool skyNeedsUpdate = true;

        // density of cirrus clouds
        float skyCloudCirrus = 0.4;
        // density of cumulus clouds
        float skyCloudCumulus = 0.8;
        // number of cumulus cloud drawing layers
        int skyCumulusLayers = 3;
        // cloud velocity factors (x = cirrus, y = cumulus)
        glm::vec2 skyCloudVelocities = glm::vec2(30., 25.);

        // nitrogen scattering coefficients (color)
        glm::vec3 skyNitrogenCoeff = glm::vec3(0.650, 0.570, 0.475);
        // atmosphere scattering coefficients (Rayleigh/Mie coeff, Mie scattering dir)
        glm::vec3 skyAtmosphereCoeff = glm::vec3(0.002, 0.0009, 0.9200);

        // when set, we re-generate sky noise and upload it to the noise texture
        bool skyNoiseNeedsUpdate = true;
        // size of the noise texture (square)
        size_t skyNoiseTextureSize = 256;
        // seed to use for sky noise texture
        int32_t skyNoiseSeed = 420;
        // frequency of sky noise
        float skyNoiseFrequency = 0.016;

        // position to input to the sky color evaluation
        glm::vec3 skyFogColorPosition = glm::vec3(0, 0.0009, 0.266);
        // color to use for the fog when the sun is below the horizon
        glm::vec3 skyNightFogColor = glm::vec3(0.03);
        // most recently calculated atmosphere color
        glm::vec3 skyAtmosphereColor;

    private:
        // Fog density and color
        float fogDensity = 0.005f;
        glm::vec3 fogColor = glm::vec3(0.53, 0.8, 0.98);
        // This is subtracted from the distance to push the fog back
        float fogOffset = 674.f;

    private:
        glm::mat4 shadowViewMatrix;

        // Scene renderer to use for rendering shadows
        std::shared_ptr<SceneRenderer> shadowSceneRenderer = nullptr;

        // Shadow texture dimensions
        unsigned int shadowW = 2048, shadowH = 2048;

        // Framebuffer object with shadow depth texture
        std::shared_ptr<gfx::FrameBuffer> shadowFbo = nullptr;
        // Shadow depth texture
        std::shared_ptr<gfx::Texture2D> shadowTex = nullptr;

        // constant to multiply directional light by
        float shadowDirectionCoefficient = 2.f;
        // "darkness" of the shadow
        float shadowFactor = 1.f;

        // ambient occlusion factor
        float ssaoFactor = 1.f;

    private:
        void drawDebugWindow();
        void drawLightsTable();

        void drawTexturePreview();

        bool showTexturePreview = false;
        glm::vec4 previewTint = glm::vec4(1);
        int previewTextureIdx = 0;
        int previewScale = 2;
};
}

#endif
