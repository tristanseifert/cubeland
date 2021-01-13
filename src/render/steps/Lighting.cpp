#include "Lighting.h"
#include "../WorldRenderer.h"
#include "../scene/SceneRenderer.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/model/RenderProgram.h"

#include "gfx/gl/texture/Texture2D.h"
#include "gfx/gl/texture/TextureCube.h"
#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/FrameBuffer.h"
#include "gfx/gl/buffer/VertexArray.h"

#include "gfx/lights/abstract/AbstractLight.h"
#include "gfx/lights/SpotLight.h"
#include "gfx/lights/DirectionalLight.h"
#include "gfx/lights/PointLight.h"

#include <Logging.h>
#include "io/Format.h"
#include "io/PrefsManager.h"

#include <mutils/time/profiler.h>
#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <FastNoise/FastNoise.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <cstdio>

using namespace render;

// vertices for a full-screen quad
static const gl::GLfloat kQuadVertices[] = {
    -1.0f,  1.0f, 0.0f,		0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,		0.0f, 0.0f,
     1.0f,  1.0f, 0.0f,		1.0f, 1.0f,
     1.0f, -1.0f, 0.0f,		1.0f, 0.0f,
};

/**
 * Initializes the lighting renderer.
 */
Lighting::Lighting() : RenderStep("Render Debug", "Lighting") {
    using namespace gfx;

    // Load the shader program
    this->program = std::make_shared<ShaderProgram>("lighting/lighting.vert", "lighting/lighting.frag");
    this->program->link();

    this->setUpRenderBuffer();

    // set up a VAO and VBO for the full-screen quad
    this->vao = std::make_shared<VertexArray>();
    this->vbo = std::make_shared<Buffer>(Buffer::Array, Buffer::StaticDraw);

    this->vao->bind();
    this->vbo->bind();

    this->vbo->bufferData(sizeof(kQuadVertices), (void *) &kQuadVertices);

    // index of vertex position
    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, 5 * sizeof(gl::GLfloat), 0);
    // index of texture sampling position
    this->vao->registerVertexAttribPointer(1, 2, VertexArray::Float, 5 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat));

    VertexArray::unbind();

    // create our lights (defaults)
    this->sun = std::make_shared<gfx::DirectionalLight>();
    this->sun->setColor(this->sunColorNormal);
    this->addLight(this->sun);

    this->moon = std::make_shared<gfx::DirectionalLight>();
    this->moon->setEnabled(false);
    this->moon->setColor(this->moonColorNormal);
    this->addLight(this->moon);

    // set up some remaining components
    this->setUpSky();
    this->setUpShadowing();

    // tell our program which texture units are used
    this->program->bind();

    this->program->setUniform1i("gNormal", this->gNormal->unit);
    this->program->setUniform1i("gDiffuse", this->gDiffuse->unit);
    this->program->setUniform1i("gMatProps", this->gMatProps->unit);
    this->program->setUniform1i("gDepth", this->gDepth->unit);
    this->program->setUniform1i("gSunShadowMap", this->shadowTex->unit);

    this->loadPrefs();
}

/**
 * Loads the preferences for rendering.
 */
void Lighting::loadPrefs() {
    this->skyEnabled = io::PrefsManager::getBool("gfx.fancySky", false);
    this->shadowEnabled = io::PrefsManager::getBool("gfx.sunShadow", true);
}

/**
 * Sets up shadowing related stuff
 */
void Lighting::setUpShadowing() {
    using namespace gfx;

    // Create FBO for shadow rendering
    this->shadowFbo = std::make_shared<FrameBuffer>();
    this->shadowFbo->bindRW();

    // Create depth texture
    this->shadowTex = std::make_shared<Texture2D>(4);
    this->shadowTex->allocateBlank(this->shadowW, this->shadowH, Texture2D::DepthGeneric);
    this->shadowTex->setBorderColour(glm::vec4(1, 1, 1, 1));
    this->shadowTex->setWrapMode(Texture2D::ClampToBorder, Texture2D::ClampToBorder);
    this->shadowTex->setUsesLinearFiltering(false);
    this->shadowTex->setDebugName("shadowMap");

    this->shadowFbo->attachTexture2D(this->shadowTex, FrameBuffer::Depth);

    // finish framebuffer
    this->shadowFbo->drawBuffersWithoutColour();

    XASSERT(FrameBuffer::isComplete(), "shadow mapping FBO incomplete");
    FrameBuffer::unbindRW();
}


/**
 * Sets up the G-buffer.
 */
void Lighting::setUpRenderBuffer() {
    using namespace gfx;

    // allocate the FBO
    this->fbo = std::make_shared<FrameBuffer>();
    this->fbo->bindRW();

    // get size of the viewport
    unsigned int width = 1024;
    unsigned int height = 768;

    // Normal vector buffer
    this->gNormal = std::make_shared<Texture2D>(0);
    this->gNormal->allocateBlank(width, height, Texture2D::RGBA16F);
    this->gNormal->setDebugName("gBufNormal");

    this->fbo->attachTexture2D(this->gNormal, FrameBuffer::ColourAttachment0);

    // Diffuse colour buffer
    this->gDiffuse = std::make_shared<Texture2D>(1);
    this->gDiffuse->allocateBlank(width, height, Texture2D::RGBA8);
    this->gDiffuse->setUsesLinearFiltering(true);
    this->gDiffuse->setDebugName("gBufDiffuse");

    this->fbo->attachTexture2D(this->gDiffuse, FrameBuffer::ColourAttachment1);

    // Material property buffer
    this->gMatProps = std::make_shared<Texture2D>(2);
    this->gMatProps->allocateBlank(width, height, Texture2D::RGBA8);
    this->gMatProps->setDebugName("gBufMatProps");

    this->fbo->attachTexture2D(this->gMatProps, FrameBuffer::ColourAttachment2);

    // Depth and stencil
    this->gDepth = std::make_shared<Texture2D>(3);
    this->gDepth->allocateBlank(width, height, Texture2D::Depth24Stencil8);
    this->gDepth->setDebugName("gBufDepth");

    this->fbo->attachTexture2D(this->gDepth, FrameBuffer::DepthStencil);

    // Specify the buffers used for rendering (sans depth)
    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::ColourAttachment1,
        FrameBuffer::ColourAttachment2,
        FrameBuffer::End
    };
    this->fbo->setDrawBuffers(buffers);


    // Ensure completeness of the buffer.
    XASSERT(FrameBuffer::isComplete(), "G-buffer FBO incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Sets up the sky renderer.
 */
void Lighting::setUpSky() {
    using namespace gfx;

    // just compile our shader. vertices are squelched from there
    this->skyProgram = std::make_shared<ShaderProgram>("lighting/sky.vert", "lighting/sky.frag");
    this->skyProgram->link();

    // sky noise texture
    this->skyNoiseTex = std::make_shared<Texture2D>(0);
    this->skyNoiseTex->allocateBlank(this->skyNoiseTextureSize, this->skyNoiseTextureSize,
            Texture2D::RED32F);
    this->skyNoiseTex->setWrapMode(Texture2D::Repeat, Texture2D::Repeat);
    this->skyNoiseTex->setDebugName("SkyNoise");

    this->generateSkyNoise();
}
/**
 * Generates the sky noise texture.
 */
void Lighting::generateSkyNoise() {
    using namespace gfx;

    // set up the noise generator and a temporary buffer
    // static const char *kNodeTree = "IwAAAIA/PQrXPg4ABgAAAAAAAEAHAAAK16M+";
    // static const char *kNodeTree="DgAGAAAAAAAAQAcAAArXoz4=";
    static const char *kNodeTree="CAA=";
    auto g = FastNoise::NewFromEncodedNodeTree(kNodeTree);

    std::vector<float> data;
    data.resize(this->skyNoiseTextureSize * this->skyNoiseTextureSize);

    g->GenTileable2D(data.data(), this->skyNoiseTextureSize, this->skyNoiseTextureSize,
            this->skyNoiseFrequency, this->skyNoiseSeed);

    // transfer it to the texture (fresh allocation _may_ help some drivers)
    this->skyNoiseTex->allocateBlank(this->skyNoiseTextureSize, this->skyNoiseTextureSize,
            Texture2D::RED32F);
    this->skyNoiseTex->bufferSubData(this->skyNoiseTextureSize, this->skyNoiseTextureSize, 0, 0, 
            Texture2D::RED32F, data.data());

    // clear flagules
    this->skyNoiseNeedsUpdate = false;
}


/**
 * Draws the debug view if enabled.
 */
void Lighting::startOfFrame() {
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
    if(this->showTexturePreview) {
        this->drawTexturePreview();
    }

    // perform sky updates
    if(this->skyEnabled && this->skyNoiseNeedsUpdate) {
        this->generateSkyNoise();
    }
}


/**
 * Sends the different lights' data to the shader, which is currently bound.
 */
void Lighting::sendLightsToShader(void) {
    using namespace gfx::lights;
    PROFILE_SCOPE(LightingSend);

    // set up counters
    int numDirectional = 0, numPoint = 0, numSpot = 0;

    // go through each type of light
    for(const auto &light : this->lights) {
        // ignore disabled, or lights whose state hasn't changed
        if(!light->isEnabled()) continue;

        // send data and increment our per-light counters
        switch(light->getType()) {
            case AbstractLight::Directional: {
                const auto i = numDirectional++;
                if(!light->isDirty() && !this->sendAllLights) continue;

                light->sendToProgram(i, this->program);
                break;
            }

            case AbstractLight::Point: {
                const auto i = numPoint++;
                if(!light->isDirty() && !this->sendAllLights) continue;

                light->sendToProgram(i, this->program);
                break;
            }

            case AbstractLight::Spot: {
                const auto i = numSpot++;
                if(!light->isDirty() && !this->sendAllLights) continue;

                light->sendToProgram(i, this->program);
                break;
            }

            default:
                Logging::warn("Invalid light type: {}", light->getType());
                break;
        }
    }

    // send how many of each type of light (directional, point, spot) we have
    glm::vec3 lightNums = glm::vec3(numDirectional, numPoint, numSpot);
    this->program->setUniformVec("LightCount", lightNums);

    this->sendAllLights = false;
}



/*
 * Resize the G-buffer as needed
 */
void Lighting::reshape(int width, int height) {
    this->gNormal->allocateBlank(width, height, gfx::Texture2D::RGBA16F);
    this->gDiffuse->allocateBlank(width, height, gfx::Texture2D::RGBA8);
    this->gMatProps->allocateBlank(width, height, gfx::Texture2D::RGBA8);
    this->gDepth->allocateBlank(width, height, gfx::Texture2D::Depth24Stencil8);
}



/**
 * Configure the OpenGL state to suit the lighting pass.
 */
void Lighting::preRender(WorldRenderer *renderer) {
    using namespace gfx;
    using namespace gl;

    // render shadow map and restore the previously bound framebuffer after
    if(this->shadowEnabled) {
        GLint drawFboId = FrameBuffer::currentDrawBuffer();
        this->renderShadowMap(renderer);
        FrameBuffer::bindDrawBufferByName(drawFboId);
    }

    // clear the output buffer
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // in case the render target has a depth buffer, DO NOT test it
    glDisable(GL_DEPTH_TEST);

    // ensure we do not write to the depth buffer during lighting
    glDepthMask(GL_FALSE);
    glStencilMask(0x00);
}

/**
 * Renders the lighting pass.
 */
void Lighting::render(WorldRenderer *renderer) {
    PROFILE_SCOPE(Lighting);

    // prepare for rendering
    gl::glViewport(0, 0, this->viewportSize.x, this->viewportSize.y);
    this->updateSunAngle(renderer);
    this->updateMoonAngle(renderer);

    // use our lighting shader, bind textures and set their locations
    this->program->bind();

    this->gNormal->bind();
    this->gDiffuse->bind();
    this->gMatProps->bind();
    this->gDepth->bind();
    this->shadowTex->bind();
    this->occlusionTex->bind();

    this->program->setUniform1i("gOcclusion", this->occlusionTex->unit);

    // Send ambient light
    this->program->setUniform1f("ambientLight.Intensity", this->ambientIntensity);
    this->program->setUniformVec("ambientLight.Colour", glm::vec3(1.0, 1.0, 1.0));

    // send the different types of light
    this->sendLightsToShader();

    // send the camera position and inverse view matrix
    this->program->setUniformVec("viewPos", this->viewPosition);

    // Inverse projection and view matrix
    glm::mat4 viewMatrixInv = glm::inverse(this->viewMatrix);
    this->program->setUniformMatrix("viewMatrixInv", viewMatrixInv);

    glm::mat4 projMatrixInv = glm::inverse(this->projectionMatrix);
    this->program->setUniformMatrix("projMatrixInv", projMatrixInv);

    // light space matrix was fucked earlier
    this->program->setUniformMatrix("lightSpaceMatrix", this->shadowViewMatrix);
    this->program->setUniform1f("shadowContribution", this->shadowFactor);
    this->program->setUniform1f("ssaoFactor", this->ssaoFactor);

    // send fog properties
    this->program->setUniform1f("fogDensity", this->fogDensity);
    this->program->setUniformVec("fogColor", this->fogColor);
    this->program->setUniform1f("fogOffset", this->fogOffset);

    // render a full-screen quad
    this->vao->bind();
    gl::glDrawArrays(gl::GL_TRIANGLE_STRIP, 0, 4);
    gfx::VertexArray::unbind();

    // unbind textures
    this->gNormal->unbind();
    this->gDiffuse->unbind();
    this->gMatProps->unbind();
    this->gDepth->unbind();
    this->shadowTex->unbind();
    this->occlusionTex->unbind();

    // render sky
    if(this->skyEnabled) {
        this->renderSky(renderer);
    }
}

/**
 * Updates the current sun angle.
 */
void Lighting::updateSunAngle(WorldRenderer *renderer) {
    bool aboveHorizon = true;

    const auto time = renderer->getTime();
    glm::vec3 sunDir(0, sin(time * M_PI_2 * 4), cos(time * M_PI_2 * 4));
    this->sunDirection = sunDir;

    if(sunDir.y >= 0) {
        this->sun->setDirection(sunDir);
        this->sun->setEnabled(true);

        this->ambientIntensity = std::min(0.25, std::max(1.33 * sunDir.y, 0.1));
    } else {
        this->sun->setEnabled(false);
        aboveHorizon = false;
    }

    /**
     * While the sun is rising/setting, we need to slowly increase the intensity of the sun, and
     * also give it a sort of orange-ish tint. Otherwise, we get a really nasty discontinuity when
     * the sun goes below the horizon and it's disabled.
     */
    if(sunDir.y >= -0.10 && sunDir.y <= 0.15) {
        const float factor = std::min((sunDir.y+0.10) * (1. / 0.25), 1.);
        const auto sunColor = glm::mix(glm::vec3(0), this->sunColorNormal, factor);
        this->sun->setColor(sunColor);
        this->sun->setEnabled(true);
    }

    /**
     * At night, we don't want shadows; in the last .15 (starting at .1) of the sun's Y position
     * range, fade out the shadows.
     */
    const float factor = std::min((this->sunDirection.y + .1) * (1. / 0.2), 1.);
    this->shadowFactor = std::max(factor, 0.f);

    /*
     * Find the angle between the camera and the sun. If we're facing the sun, we'll be getting a
     * bunch of whacky bright colors to match the horizon of the rising sun; if we're facing away,
     * it should stay pretty dark.
     *
     * We consider to be facing away from the sun when the angle is greater than 1.9*FoV.
     */
    bool needFogMix = false;
    auto angle = glm::angle(this->viewDirection, this->sunDirection);
    auto angleDiff = angle - (1.8 * glm::radians(renderer->getFoV()));
    auto sunMixFactor = std::min(std::max(0., angleDiff), 1.);

    if(this->sunDirection.y <= 0.09) {
        needFogMix = true;
        sunMixFactor *= 1 - std::min(this->sunDirection.y * (1. / 0.09), 1.);
    }

    // with the new sun angle, calculate the fog color (atmosphere color)
    glm::vec3 color;
    if(aboveHorizon) {
        float Br = this->skyAtmosphereCoeff.x;
        float Bm = this->skyAtmosphereCoeff.y;
        float g =  this->skyAtmosphereCoeff.z;

        const glm::vec3 Kr = Br / glm::pow(this->skyNitrogenCoeff, glm::vec3(4.0));
        const glm::vec3 Km = Bm / glm::pow(this->skyNitrogenCoeff, glm::vec3(0.84));

        const glm::vec3 pos = this->skyFogColorPosition;
        const float mu = glm::dot(glm::normalize(pos), glm::normalize(this->sunDirection));

        const glm::vec3 extinction = glm::mix(glm::exp(-glm::exp(-((pos.y + sunDir.y * 4.f) * (glm::exp(-pos.y * 16.f) + 0.1f) / 80.f) / Br) * (glm::exp(-pos.y * 16.f) + 0.1f) * Kr / Br) * glm::exp(-pos.y * exp(-pos.y * 8.f) * 4.f) * glm::exp(-pos.y * 2.f) * 4.f, glm::vec3(1.f - glm::exp(sunDir.y)) * 0.2f, -sunDir.y * 0.2f + 0.5f);
        const auto out = 3.f / (8.f * 3.14f) * (1.f + mu * mu) * (Kr + Km * (1.f - g * g) / (2.f + g * g) / glm::pow(1.f + g * g - 2.f * g * mu, 1.5f)) / (Br + Bm) * extinction;

        color = glm::pow(1.f - glm::exp(-1.3f * color), glm::vec3(-1.3));
        color = glm::normalize(out);
        this->skyAtmosphereColor = color;
    }
    // sun is below horizon, fade to deep black
    else {
        const float factor = abs(sunDir.y * 9.2);
        color = glm::mix(this->skyAtmosphereColor, this->skyNightFogColor, std::min(factor, 1.f));
    }

    color = glm::max(color, glm::vec3(0));

    if(needFogMix) {
        this->fogColor = glm::mix(color, this->skyNightFogColor, std::min(0.66, sunMixFactor));
    } else {
        this->fogColor = color;
    }
}
/**
 * Updates the moon angle. This is currently just used for the moon light.
 */
void Lighting::updateMoonAngle(WorldRenderer *renderer) {
    bool aboveHorizon = true;

    const auto time = renderer->getTime();
    glm::vec3 moonDir(0, -sin(time * M_PI_2 * 4), -cos(time * M_PI_2 * 4));
    this->moonDirection = moonDir;

    if(moonDir.y >= 0) {
        this->moon->setDirection(moonDir);
        this->moon->setEnabled(true);

        this->ambientIntensity = std::min(0.133, std::max(0.3 * moonDir.y, 0.1));
    } else {
        this->moon->setEnabled(false);
        aboveHorizon = false;
    }

    // interpolate its color
    if(this->moonDirection.y >= -0.03 && this->moonDirection.y <= 0.15) {
        const float factor = std::min((this->moonDirection.y + .03) * (1. / 0.18), 1.);
        const auto moonColor = glm::mix(glm::vec3(0), this->moonColorNormal, factor);
        this->moon->setColor(moonColor);
        this->moon->setEnabled(true);
    }
}
/**
 * Draws the sky itself.
 */
void Lighting::renderSky(WorldRenderer *r) {
    using namespace gfx;
    using namespace gl;
    PROFILE_SCOPE(CloudsSky);

    // bind program
    this->skyProgram->bind();

    // glm::mat4 newView = glm::mat4(glm::mat3(this->viewMatrix));
    this->skyProgram->setUniformMatrix("view", this->viewMatrix);
    this->skyProgram->setUniformMatrix("projection", this->projectionMatrix);
    this->skyProgram->setUniform1f("time", r->getTime());
    this->skyProgram->setUniformVec("sunPosition", this->sunDirection);

    if(this->skyNeedsUpdate) {
        this->skyProgram->setUniform1f("cirrus", this->skyCloudCirrus);
        this->skyProgram->setUniform1f("cumulus", this->skyCloudCumulus);
        this->skyProgram->setUniform1i("numCumulus", this->skyCumulusLayers);
        this->skyProgram->setUniformVec("cloudVelocities", this->skyCloudVelocities);
        this->skyProgram->setUniformVec("nitrogen", this->skyNitrogenCoeff);
        this->skyProgram->setUniformVec("scatterCoeff", this->skyAtmosphereCoeff);
        // this->skyProgram->setUniform1i("noiseTex", this->skyNoiseTex->unit);
    }

    // draw a full screen quad, WITH depth testing, behind everything else
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // glDepthFunc(GL_LESS);

    this->skyNoiseTex->bind();
    this->vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    VertexArray::unbind();
}

/**
 * Restores the previous rendering state.
 */
void Lighting::postRender(WorldRenderer *) {
    using namespace gl;

    // allow successive render passes to render depth
    glDepthMask(GL_TRUE);

    // re-enable depth testing
    glEnable(GL_DEPTH_TEST);
}



/**
 * Binds the various G-buffer elements before the scene itself is rendered. This sets up three
 * textures, into which the following data is rendered:
 *
 * 1. Positions (RGB)
 * 2. Color (RGB) plus specular (A)
 * 3. Normal vectors (RGB)
 * 4. Material properties
 *
 * Following a call to this function, the scene should be rendered, and when this technique is
 * rendered, it will render the final geometry with lighting applied.
 *
 * A depth buffer with at least an 8-bit stencil is also atacched. The usage of stencil bits is
 * as follows:
 * - Bit 0: Chunk outlines
 */
void Lighting::bindGBuffer(void) {
    this->fbo->bindRW();

    // re-attach the depth texture
    this->fbo->attachTexture2D(this->gDepth, gfx::FrameBuffer::DepthStencil);
    XASSERT(gfx::FrameBuffer::isComplete(), "G-buffer FBO incomplete");
}

/**
 * Unbinds the gbuffer.
 */
void Lighting::unbindGBuffer(void) {
    this->fbo->unbindRW();
}



/**
 * Adds a light to the list of lights. Each frame, these lights are sent to the GPU.
 */
void Lighting::addLight(std::shared_ptr<gfx::lights::AbstractLight> light) {
    this->lights.push_back(light);
}

/**
 * Removes a previously added light.
 */
void Lighting::removeLight(std::shared_ptr<gfx::lights::AbstractLight> light) {
    this->lights.erase(std::remove(this->lights.begin(), this->lights.end(), light), this->lights.end());
    this->sendAllLights = true;
}



// 'optionalPMatrixInverse16' is required only if you need to retrieve (one or more of) the arguments that follow it (otherwise their value is untouched).
static void GetLightViewProjectionMatrixExtra(glm::mat4 &lvpMatrixOut16,
                                                const glm::mat4 &cameraVMatrixInverse16,
                                                float cameraNearClippingPlane,
                                                float cameraFarClippingPlane,
                                                float cameraFovyDeg,
                                                float cameraAspectRatio,
                                                float cameraTargetDistanceForUnstableOrtho3DModeOnly_or_zero,
                                                const glm::vec3 &normalizedLightDirection3, 
                                                float texelIncrement,
                                                float *optionalSphereCenterOut,
                                                float *optionalSphereRadiiSquaredOut,
                                                const glm::mat4 *optionalCameraPMatrixInverse16,
                                                glm::vec4 *optionalLightViewportClippingOut4,
                                                float optionalCameraFrustumPointsInNDCLightSpaceOut[8][4],
                                                float* optionalLVPMatrixForFrustumCullingUsageOut16   // Highly experimental and untested
                                                          )  {
    // const glm::vec3 cameraPosition3(cameraVMatrixInverse16[12], cameraVMatrixInverse16[13], cameraVMatrixInverse16[14]);
    const glm::vec3 cameraPosition3(cameraVMatrixInverse16[3][0], cameraVMatrixInverse16[3][1], cameraVMatrixInverse16[3][2]);
    // const glm::vec3 cameraForwardDirection3(-cameraVMatrixInverse16[8],-cameraVMatrixInverse16[9],-cameraVMatrixInverse16[10]);
    const glm::vec3 cameraForwardDirection3(-cameraVMatrixInverse16[2][0],-cameraVMatrixInverse16[2][1], -cameraVMatrixInverse16[2][2]);

    glm::vec3 frustumCenter(0);
    float radius = 0;
    glm::mat4 lpMatrix(1), lvMatrix(1);

    float frustumCenterDistance,tanFovDiagonalSquared;
    const float halfNearFarClippingPlane = 0.5*(cameraFarClippingPlane+cameraNearClippingPlane);

    if (cameraTargetDistanceForUnstableOrtho3DModeOnly_or_zero>cameraFarClippingPlane) cameraTargetDistanceForUnstableOrtho3DModeOnly_or_zero = 0;  // Not needed

    // Get frustumCenter and radius
    tanFovDiagonalSquared = tan(cameraFovyDeg*M_PI/360.0); // At this point this is just TANFOVY
    if (cameraTargetDistanceForUnstableOrtho3DModeOnly_or_zero<=0)  {
        // camera perspective mode here
        tanFovDiagonalSquared*=tanFovDiagonalSquared;
        tanFovDiagonalSquared*=(1.0+cameraAspectRatio*cameraAspectRatio);
        frustumCenterDistance = halfNearFarClippingPlane*(1.0+tanFovDiagonalSquared);
        if (frustumCenterDistance > cameraFarClippingPlane) frustumCenterDistance = cameraFarClippingPlane;
        radius = (tanFovDiagonalSquared*cameraFarClippingPlane*cameraFarClippingPlane) + (cameraFarClippingPlane-frustumCenterDistance)*(cameraFarClippingPlane-frustumCenterDistance); // This is actually radiusSquared
    }
    else {
        // camera ortho3d mode here
        const float y=cameraTargetDistanceForUnstableOrtho3DModeOnly_or_zero*tanFovDiagonalSquared;
        const float x=y*cameraAspectRatio;
        const float halfClippingPlaneDistance = 0.5*(cameraFarClippingPlane-cameraNearClippingPlane);
        frustumCenterDistance = halfNearFarClippingPlane;
        radius = x*x+y*y; // This is actually radiusXYSquared
        radius = radius + halfClippingPlaneDistance*halfClippingPlaneDistance;// This is actually radiusSquared
    }
    for (size_t i = 0; i < 3; i++) {
        frustumCenter[i] = cameraPosition3[i]+cameraForwardDirection3[i]*frustumCenterDistance;
    }

    if (optionalSphereCenterOut)        *optionalSphereCenterOut = frustumCenterDistance;
    if (optionalSphereRadiiSquaredOut)  *optionalSphereRadiiSquaredOut = radius;
    radius = sqrt(radius);
    //fprintf(stderr,"radius=%1.4f frustumCenterDistance=%1.4f nearPlane=%1.4f farPlane = %1.4f\n",radius,frustumCenterDistance,cameraNearClippingPlane,cameraFarClippingPlane);

    // For people trying to save texture space it's:  halfNearFarClippingPlane <= frustumCenterDistance <= cameraFarClippingPlane
    // When frustumCenterDistance == cameraFarClippingPlane, then frustumCenter is on the far clip plane (and half the texture space gets wasted).
    // when frustumCenterDistance == halfNearFarClippingPlane, then we're using an ortho projection matrix, and frustumCenter is in the middle of the near and far plane (no texture space gets wasted).
    // in all the other cases the space wasted can go from zero to half texture

    // Shadow swimming happens when: 1) camera translates; 2) camera rotates; 3) objects move or rotate
    // AFAIK Shadow swimming (3) can't be fixed in any way
    if (texelIncrement>0)   radius = ceil(radius/texelIncrement)*texelIncrement;      // This 'should' fix Shadow swimming (1)  [Not sure code is correct!]

    // Get light matrices
    lpMatrix = glm::ortho(-radius, radius, -radius, radius, 0.f, -2.f * radius);

    const glm::vec3 eye(frustumCenter[0]-normalizedLightDirection3[0]*radius,
            frustumCenter[1]-normalizedLightDirection3[1]*radius,
            frustumCenter[2]-normalizedLightDirection3[2]*radius);
    lvMatrix = glm::lookAt(eye, frustumCenter, glm::vec3(0, 1, 0));

    Logging::trace("radius {}, eye {} center {}", radius, eye, frustumCenter);

    // Get output
    // lvpMatrixOut16 = lpMatrix * lvMatrix;
    lvpMatrixOut16 = lvMatrix * lpMatrix;

    // This 'should' fix Shadow swimming (2) [Not sure code is correct!]
    /*if (texelIncrement>0)   {
        float shadowOrigin[4]   = {0,0,0,1};
        float roundedOrigin[4]  = {0,0,0,0};
        float roundOffset[4]    = {0,0,0,0};
        float texelCoefficient = texelIncrement*2.0;
        // Helper_MatrixMulPos(lvpMatrixOut16,shadowOrigin,shadowOrigin[0],shadowOrigin[1],shadowOrigin[2]);

        for (size_t i = 0; i < 2; i++) {// Or i<3 ?
            shadowOrigin[i]/= texelCoefficient;
            roundedOrigin[i] = round(shadowOrigin[i]);
            roundOffset[i] = roundedOrigin[i] - shadowOrigin[i];
            roundOffset[i]*=  texelCoefficient;
        }

        lvpMatrixOut16[3][0] += roundOffset[0];
        lvpMatrixOut16[3][1]+= roundOffset[1];
    }*/

    // Debug stuff
    //fprintf(stderr,"radius=%1.5f frustumCenter={%1.5f,%1.5f,%1.5f}\n",radius,frustumCenter[0],frustumCenter[1],frustumCenter[2]);

    // Extra stuff [Not sure code is correct: the returned viewport seems too big!]
    /*if (optionalCameraPMatrixInverse16) {
        int j;
        hloat cameraVPMatrixInv[16],cameraVPMatrixInverseAdjusted[16];hloat frustumPoints[8][4];
        hloat minVal[3],maxVal[3],tmp;
        Helper_MultMatrix(cameraVPMatrixInv,cameraVMatrixInverse16,optionalCameraPMatrixInverse16); // vMatrixInverse16 needs an expensive Helper_InvertMatrix(...) to be calculated. Here we can exploit the property of the product of 2 invertse matrices.
        // If we call Helper_GetFrustumPoints(frustumPoints,cameraVPMatrixInv) we find the frustum corners in world space

        Helper_MultMatrix(cameraVPMatrixInverseAdjusted,lvpMatrixOut16,cameraVPMatrixInv);  // This way we 'should' get all points in the [-1,1] light NDC space (or not?)

        Helper_GetFrustumPoints(frustumPoints,cameraVPMatrixInverseAdjusted);

        if (optionalCameraFrustumPointsInNDCLightSpaceOut) {
            for (i=0;i<8;i++)   {
                for (j=0;j<4;j++)   {
                    optionalCameraFrustumPointsInNDCLightSpaceOut[i][j] = frustumPoints[i][j];
                }
            }
        }

        // Calculate 'minVal' and 'maxVal' based on 'frustumPoints'
        for (i=0;i<3;i++)   minVal[i]=maxVal[i]=frustumPoints[0][i];
        for (i=1;i<8;i++)   {
            for (j=0;j<3;j++)   {   // We will actually skip the z component later...
                tmp = frustumPoints[i][j];
                if      (minVal[j]>tmp) minVal[j]=tmp;
                else if (maxVal[j]<tmp) maxVal[j]=tmp;
            }
            //fprintf(stderr,"frustumPoints[%d]={%1.4f,%1.4f,%1.4f}\n",i,frustumPoints[i][0], frustumPoints[i][1], frustumPoints[i][2]);
        }

        if (optionalLightViewportClippingOut4)   {
            optionalLightViewportClippingOut4[0] = minVal[0]*0.5+0.5;   // In [0,1] from [-1,1]
            optionalLightViewportClippingOut4[1] = minVal[1]*0.5+0.5;   // In [0,1] from [-1,1]
            optionalLightViewportClippingOut4[2] = (maxVal[0]-minVal[0])*0.5;    // extent x in [0,1]
            optionalLightViewportClippingOut4[3] = (maxVal[1]-minVal[1])*0.5;    // extent y in [0,1]

            for (i=0;i<4;i++)   {
               optionalLightViewportClippingOut4[i]/=texelIncrement;    // viewport is in [0,texture_size]
            }

            // optionalLightViewportClippingOut4[0] = floor(optionalLightViewportClippingOut4[0]);
            // optionalLightViewportClippingOut4[1] = floor(optionalLightViewportClippingOut4[1]);
            // optionalLightViewportClippingOut4[2] = ceil(optionalLightViewportClippingOut4[2]);
            // optionalLightViewportClippingOut4[3] = ceil(optionalLightViewportClippingOut4[3]);

            //fprintf(stderr,"viewport={%1.4f,%1.4f,%1.4f,%1.4f}\n",optionalLightViewportClippingOut4[0],optionalLightViewportClippingOut4[1],optionalLightViewportClippingOut4[2],optionalLightViewportClippingOut4[3]);
        }

        if (optionalLVPMatrixForFrustumCullingUsageOut16)   {
            const int attemptToFixSwimming = (lvpMatrixOut16==lvpMatrixFallback) ? 1 : 0;   // Only if we don't want lvpMatrixOut16
            float minmaxXY[4]={minVal[0]*radius,maxVal[0]*radius,minVal[1]*radius,maxVal[1]*radius};
            if (attemptToFixSwimming && texelIncrement>0)   {
                for (i=0;i<4;i++) {
                    // This 'should' fix Shadow swimming (1) in the 'Stable Shadow Mapping Technique'
                    // Not sure it works here too...
                    if (minmaxXY[i]>=0) minmaxXY[i] = ceil(minmaxXY[i]/texelIncrement)*texelIncrement;
                    else                minmaxXY[i] = -ceil(-minmaxXY[i]/texelIncrement)*texelIncrement;
                }
            }
            Helper_Ortho(optionalLVPMatrixForFrustumCullingUsageOut16,
                         minmaxXY[0],minmaxXY[1],
                         minmaxXY[2],minmaxXY[3],
                         0,-2.0*radius                      // For z, we just copy Helper_Ortho(lpMatrix,...)
                         );
            Helper_MultMatrix(optionalLVPMatrixForFrustumCullingUsageOut16,optionalLVPMatrixForFrustumCullingUsageOut16,lvMatrix);
            // This 'should' fix Shadow swimming (2) in the 'Stable Shadow Mapping Technique'
            // Not here, because the shadow viewport stretches when the camera rotates
            // We try anyway...
            if (attemptToFixSwimming && texelIncrement>0)   {
                hloat shadowOrigin[4]   = {0,0,0,1};
                hloat roundedOrigin[4]  = {0,0,0,0};
                hloat roundOffset[4]    = {0,0,0,0};
                hloat texelCoefficient = texelIncrement*2.0;
                Helper_MatrixMulPos(optionalLVPMatrixForFrustumCullingUsageOut16,shadowOrigin,shadowOrigin[0],shadowOrigin[1],shadowOrigin[2]);
                for (i = 0; i < 2; i++) {// Or i<3 ?
                    shadowOrigin[i]/= texelCoefficient;
                    roundedOrigin[i] = Helper_Round(shadowOrigin[i]);
                    roundOffset[i] = roundedOrigin[i] - shadowOrigin[i];
                    roundOffset[i]*=  texelCoefficient;
                }
                optionalLVPMatrixForFrustumCullingUsageOut16[12]+= roundOffset[0];
                optionalLVPMatrixForFrustumCullingUsageOut16[13]+= roundOffset[1];
            }
        }
    }*/
}




static void GetLightViewProjectionMatrix(glm::mat4 &lvpMatrixOut16,
        const glm::mat4 &cameraVMatrixInverse16, const float cameraNearClippingPlane,
        const float cameraFarClippingPlane, const float cameraFovyDeg,
        const float cameraAspectRatio, const glm::vec3 &normalizedLightDirection3, 
        const float texelIncrement)  {
    GetLightViewProjectionMatrixExtra(lvpMatrixOut16,cameraVMatrixInverse16,cameraNearClippingPlane,cameraFarClippingPlane,cameraFovyDeg,cameraAspectRatio,0,normalizedLightDirection3,texelIncrement,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}


/**
 * Renders the shadow map.
 *
 * The position of the camera for shadow mapping is calcualted by taking the
 * actual camera position, getting its position, and adding to it the direction
 * of the directional light (i.e. sun) multiplied by a certain factor.
 */
void Lighting::renderShadowMap(WorldRenderer *wr) {
    // TODO: fix this
    return;

    using namespace gl;
    using namespace gfx;
    PROFILE_SCOPE(LightingShadow);

    // bail out if shadow factor is 0
    if(this->shadowFactor <= 0) {
        return;
    }

    // back up last viewport
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);

    // get Z plane
    // TODO: Tweak this slightly
    // const glm::vec2 zPlane = wr->getZPlane();
    //const float zNear = zPlane.x;
    //const float zFar = zPlane.y;
    const float zNear = .5;
    const float zFar = 100.;
    const glm::vec3 lightDir = normalize(this->sunDirection);

    // Calculate shadow view matrix
    /*const float aspect = wr->getAspect();
    const auto &viewInv = inverse(this->viewMatrix);
    glm::mat4 lvpMatrix;
    GetLightViewProjectionMatrix(lvpMatrix, viewInv, zNear, zFar, wr->getFoV(), aspect,
            normalize(lightDir), 1.f / ((float) this->shadowW));
    this->shadowViewMatrix = lvpMatrix;*/

    glm::vec3 position = wr->getCamera().getCameraPosition();
    glm::mat4 depthProjectionMatrix = glm::ortho<float>(-100, 100, -100, 100, zNear, zFar);

    position.y = 64;
    glm::vec3 shadowPos = position - (lightDir * 100.f / 2.f);
    glm::mat4 depthViewMatrix = glm::lookAt(shadowPos, position, glm::vec3(0,1,0));

    glm::mat4 lightSpaceMatrix = depthProjectionMatrix * depthViewMatrix;
    this->shadowViewMatrix = lightSpaceMatrix;

    // set viewport and a few other GL properties
    this->shadowFbo->bindRW();

    glViewport(0, 0, this->shadowW, this->shadowH);

    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // set up culling: prevents peter panning
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // render the scene
    this->shadowSceneRenderer->render(this->shadowViewMatrix, lightDir, true, false);

    // reset viewport
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glCullFace(GL_BACK);
}



/**
 * Draws the lighting renderer debug window
 */
void Lighting::drawDebugWindow() {
    bool reallocShadows = false;
    int i;

    // combo box constant valuies
    constexpr static const size_t kNumShadowSizes = 5;
    static const char *kShadowSizes[kNumShadowSizes] = {
        "256", "512", "1024", "2048", "4096"
    };

    // short circuit drawing if not visible
    if(!ImGui::Begin("Lighting Renderer", &this->showDebugWindow)) {
        goto done;
    }

    // tools
    if(ImGui::Button("Buffers")) {
        this->showTexturePreview = true;
    }

    ImGui::Separator();

    // sky
    ImGui::Text("Sky");
    ImGui::Separator();
    ImGui::PushItemWidth(74);

    if(ImGui::Checkbox("Enabled##sky", &this->skyEnabled)) {
        this->skyNeedsUpdate = true;
    }

    if(ImGui::DragFloat("Cirrus Density", &this->skyCloudCirrus, 0.01, 0, 1, "%.4f")) {
        this->skyNeedsUpdate = true;
    }
    if(ImGui::DragFloat("Cumulus Density", &this->skyCloudCumulus, 0.01, 0, 1, "%.4f")) {
        this->skyNeedsUpdate = true;
    }
    if(ImGui::DragFloat2("Cloud Velocity", &this->skyCloudVelocities.x, 0.01, 0)) {
        this->skyNeedsUpdate = true;
    }
    if(ImGui::DragInt("Cumulus Layers", &this->skyCumulusLayers, 1, 1)) {
        this->skyNeedsUpdate = true;
    }

    ImGui::PushItemWidth(150);
    if(ImGui::ColorEdit3("Nitrogen Color", &this->skyNitrogenCoeff.x)) {
        this->skyNeedsUpdate = true;
    }

    ImGui::ColorEdit3("Night Fog", &this->skyNightFogColor.x);
    ImGui::PopItemWidth();

    if(ImGui::DragFloat("Rayleigh Coefficient", &this->skyAtmosphereCoeff.x, 0.001, 0, 1, "%.4f")) {
        this->skyNeedsUpdate = true;
    }
    if(ImGui::DragFloat("Mie Coefficient", &this->skyAtmosphereCoeff.y, 0.001, 0, 1, "%.4f")) {
        this->skyNeedsUpdate = true;
    }
    if(ImGui::DragFloat("Mie Scatter Direction", &this->skyAtmosphereCoeff.z, 0.001, 0, 1, "%.4f")) {
        this->skyNeedsUpdate = true;
    }

    if(ImGui::DragFloat("Noise Frequency", &this->skyNoiseFrequency, 0.001, -1, 1, "%.4f")) {
        this->skyNoiseNeedsUpdate = true;
    }
    if(ImGui::DragInt("Noise Seed", &this->skyNoiseSeed)) {
        this->skyNoiseNeedsUpdate = true;
    }

    ImGui::PushItemWidth(150);
    ImGui::DragFloat3("Fog Position", &this->skyFogColorPosition.x, 0.001);
    ImGui::DragFloat3("Sun Direction", &this->sunDirection.x, 0.001);
    ImGui::DragFloat3("Moon Direction", &this->moonDirection.x, 0.001);
    ImGui::PopItemWidth();

    // fog
    ImGui::Text("Fog");
    ImGui::Separator();

    ImGui::DragFloat("Density", &this->fogDensity, 0.02, 0);
    ImGui::DragFloat("Offset", &this->fogOffset, 0.1, 0);

    ImGui::PushItemWidth(150);
    ImGui::ColorEdit3("Color", &this->fogColor.x);
    ImGui::PopItemWidth();

    // shadow section
    ImGui::Text("Shadows");
    ImGui::Separator();

    ImGui::PushItemWidth(74);
    ImGui::DragFloat("Shadow Dir Coefficient", &this->shadowDirectionCoefficient, 0.02, 0.1, 6);

    ImGui::DragFloat("Shadow Coefficient", &this->shadowFactor, 0.02, 0, 1);
    ImGui::DragFloat("Ambient Occlusion Coefficient", &this->ssaoFactor, 0.02, 0, 1);

    i = std::max(std::min((int) ((log10(this->shadowW) / log10(2)) - 8), 4), 0);
    if(ImGui::BeginCombo("Shadow Map Size", kShadowSizes[i])) {
        for(size_t j = 0; j < kNumShadowSizes; j++) {
            const bool isSelected = (i == j);

            if (ImGui::Selectable(kShadowSizes[j], isSelected)) {
                this->shadowW = pow(2, 8+j);
                this->shadowH = pow(2, 8+j);
                reallocShadows = true;
            }
            if(isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    if(reallocShadows) {
        Logging::info("New shadow map size: {}x{}", this->shadowW, this->shadowH);
        this->shadowTex->allocateBlank(this->shadowW, this->shadowH, gfx::Texture2D::DepthGeneric);
    }

    // configured lights
    ImGui::Text("Lights");
    ImGui::Separator();

    ImGui::DragFloat("Ambient Intensity", &this->ambientIntensity, 0.001, 0);

    ImGui::PushItemWidth(150);
    ImGui::ColorEdit3("Sunlight Color", &this->sunColorNormal.x);
    ImGui::PopItemWidth();

    ImGui::PopItemWidth();
    this->drawLightsTable();

done:;
    ImGui::End();
}

/**
 * Draws the table that contains detail info on each of our lights.
 */
void Lighting::drawLightsTable() {
    using namespace gfx::lights;

    if(ImGui::BeginTable("lights", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableColumnFlags_WidthStretch)) {
        // headers
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 25);
        ImGui::TableSetupColumn("Diffuse");
        ImGui::TableSetupColumn("Specular");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Direction");
        ImGui::TableSetupColumn("Attenuation");
        ImGui::TableHeadersRow();

        // per light info
        int i = 0;
        for(const auto &light : this->lights) {
            ImGui::TableNextRow();

            // labels for each component
            ImGui::PushID(i++);

            // render based on light type
            switch(light->getType()) {
                case AbstractLight::Ambient:
                    ImGui::TableNextColumn();
                    ImGui::Text("A");
                    if(ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Ambient");
                    }

                    ImGui::PushItemWidth(175);

                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##diff", &light->diffuseColor.x, 0.01, -10, 10)) light->markDirty();
                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##spec", &light->specularColor.x, 0.01, -10, 10)) light->markDirty();

                    ImGui::PopItemWidth();
                    break;

                case AbstractLight::Directional: {
                    auto dir = std::dynamic_pointer_cast<gfx::DirectionalLight>(light);

                    ImGui::TableNextColumn();
                    ImGui::Text("D");
                    if(ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Directional");
                    }

                    ImGui::PushItemWidth(175);

                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##diff", &light->diffuseColor.x, 0.01, -10, 10)) light->markDirty();
                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##spec", &light->specularColor.x, 0.01, -10, 10)) light->markDirty();

                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("-");

                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##dir", &dir->direction.x, 0.01)) light->markDirty();

                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("-");
                    break;
                }

                case AbstractLight::Point: {
                    auto pt = std::dynamic_pointer_cast<gfx::PointLight>(light);

                    ImGui::TableNextColumn();
                    ImGui::Text("P");
                    if(ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Point");
                    }

                    ImGui::PushItemWidth(175);

                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##diff", &light->diffuseColor.x, 0.01, -10, 10)) light->markDirty();
                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##spec", &light->specularColor.x, 0.01, -10, 10)) light->markDirty();

                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat3("##pos", &pt->position.x)) light->markDirty();

                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("-");

                    ImGui::TableNextColumn();
                    if(ImGui::DragFloat("##linear", &pt->linearAttenuation, 0.001, 0.01)) light->markDirty();
                    if(ImGui::DragFloat("##quad", &pt->quadraticAttenuation, 0.001, 0.01)) light->markDirty();
                    // ImGui::Text("%.3g/%.3g", pt->linearAttenuation, pt->quadraticAttenuation);
                    break;
                }

                default:
                    ImGui::TableNextColumn();
                    ImGui::Text("?");
                    break;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

/**
 * Draws the texture preview window.
 */
void Lighting::drawTexturePreview() {
    static const char *kPreviewName[5] = {
        "G Diffuse", "G Normal", "G Material", "G Depth", "Shadow Depth",
    };

    auto &io = ImGui::GetIO();
  // short circuit drawing if not visible
    if(!ImGui::Begin("Lighting Buffers", &this->showTexturePreview, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // toolbar
    ImGui::PushItemWidth(200);
    ImGui::ColorEdit4("Tint", &this->previewTint.x);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10,0));
    ImGui::SameLine();
    ImGui::PushItemWidth(32);
    ImGui::DragInt("Scale", &this->previewScale, 1, 1, 16);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10,0));
    ImGui::PushItemWidth(133);
    ImGui::SameLine();
    if(ImGui::BeginCombo("Buffer", kPreviewName[this->previewTextureIdx])) {
        for(size_t j = 0; j < 5; j++) {
            const bool isSelected = (this->previewTextureIdx == j);

            if (ImGui::Selectable(kPreviewName[j], isSelected)) {
                this->previewTextureIdx = j;
            }
            if(isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // draw pls
    ImGui::PopItemWidth();
    ImGui::Separator();

    ImVec2 uv0(0,1), uv1(1,0);
    ImVec2 imageSize(this->viewportSize.x, this->viewportSize.y);
    ImVec4 tint(this->previewTint.x, this->previewTint.y, this->previewTint.z, this->previewTint.w);

    gl::GLuint textureId = this->gDiffuse->getGlObjectId();
    if(this->previewTextureIdx == 1) {
        textureId = this->gNormal->getGlObjectId();
    } else if(this->previewTextureIdx == 2) {
        textureId = this->gMatProps->getGlObjectId();
    } else if(this->previewTextureIdx == 3) {
        textureId = this->gDepth->getGlObjectId();
    } else if(this->previewTextureIdx == 4) {
        textureId = this->shadowTex->getGlObjectId();
        imageSize = glm::vec2(this->shadowW, this->shadowH);
    }

    const auto textureSize = imageSize;

    imageSize.x = imageSize.x / this->previewScale / io.DisplayFramebufferScale.x;
    imageSize.y = imageSize.y / this->previewScale / io.DisplayFramebufferScale.y;

    ImGui::Image((void *) (size_t) textureId, imageSize, uv0, uv1, tint, ImVec4(1,1,1,1));

    // image info
    ImGui::Text("Texture Size: %g x %g", textureSize.x, textureSize.y);
    ImGui::SameLine();
    ImGui::Text("Display Size: %g x %g", imageSize.x, imageSize.y);

    // done
    ImGui::End();
}
