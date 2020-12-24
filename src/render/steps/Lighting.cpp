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

// test data
static const glm::vec3 cubeLightColors[] = {
	glm::vec3(1.0f, 0.0f, 0.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	glm::vec3(0.0f, 0.0f, 1.0f),
	glm::vec3(1.0f, 0.5f, 0.0f) * 10.f,
/*	glm::vec3(176.f/255.f, 23.f/255.f, 31.f/255.f),
	glm::vec3(0.5f, 0.0f, 0.5f),
	glm::vec3(0.0f, 1.0f, 1.0f),
	glm::vec3(1.0f, 1.0f, 0.0f),
	glm::vec3(1.0f, 0.5f, 0.0f),
	glm::vec3(113.f/255.f, 198.f/255.f, 113.f/255.f),
	glm::vec3(1.0f, 0.8f, 0.8f),*/
};

static const glm::vec3 cubeLightPositions[] = {
    glm::vec3( 1.5f,  2.0f, -2.5f),
    glm::vec3( 1.5f,  0.2f, -1.5f),
    glm::vec3(-1.3f,  1.0f, -1.5f),
    glm::vec3( 1.5f,  2.0f, -1.5f)
};

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
Lighting::Lighting() {
    using namespace gfx;

    // Load the shader program
    this->program = std::make_shared<ShaderProgram>("/lighting/lighting.vert", "/lighting/lighting.frag");
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
    this->addLight(this->sun);

    this->moon = std::make_shared<gfx::DirectionalLight>();
    this->moon->setEnabled(false);
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

    this->showDebugWindow = true;
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
    this->skyProgram = std::make_shared<ShaderProgram>("/lighting/sky.vert", "/lighting/sky.frag");
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

    auto range = g->GenTileable2D(data.data(), this->skyNoiseTextureSize, this->skyNoiseTextureSize,
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
    int numDirectional, numPoint, numSpot;
    numDirectional = numPoint = numSpot = 0;

    // go through each type of light
    for(const auto &light : this->lights) {
        // ignore disabled, or lights whose state hasn't changed
        if(!light->isEnabled()) continue;
        if(!light->isDirty()) continue;

        // send data and increment our per-light counters
        switch(light->getType()) {
            case AbstractLight::Directional:
                light->sendToProgram(numDirectional++, this->program);
                break;

            case AbstractLight::Point:
                light->sendToProgram(numPoint++, this->program);
                break;

            case AbstractLight::Spot:
                light->sendToProgram(numSpot++, this->program);
                break;

            default:
                Logging::warn("Invalid light type: {}", light->getType());
                break;
        }
    }

    // send how many of each type of light (directional, point, spot) we have
    glm::vec3 lightNums = glm::vec3(numDirectional, numPoint, numSpot);
    this->program->setUniformVec("LightCount", lightNums);
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
    GLint drawFboId = FrameBuffer::currentDrawBuffer();
    this->renderShadowMap(renderer);
    FrameBuffer::bindDrawBufferByName(drawFboId);

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
    PROFILE_SCOPE(LightingRender);

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

        this->ambientIntensity = std::min(0.74, std::max(1.33 * sunDir.y, 0.1));
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

        this->ambientIntensity = std::min(0.2, std::max(0.3 * moonDir.y, 0.1));
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

    glm::mat4 newView = glm::mat4(glm::mat3(this->viewMatrix));
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
    this->lights.push_back(light);;
}

/**
 * Removes a previously added light.
 *
 * @return 0 if the light was removed, -1 otherwise.
 */
int Lighting::removeLight(std::shared_ptr<gfx::lights::AbstractLight> light) {
    auto position = std::find(this->lights.begin(), this->lights.end(), light);

    if(position > this->lights.end()) {
        return -1;
    } else {
        this->lights.erase(position);
        return 0;
    }
}



/**
 * Renders the shadow map.
 *
 * The position of the camera for shadow mapping is calcualted by taking the
 * actual camera position, getting its position, and adding to it the direction
 * of the directional light (i.e. sun) multiplied by a certain factor.
 */
void Lighting::renderShadowMap(WorldRenderer *wr) {
    using namespace gl;
    using namespace gfx;
    PROFILE_SCOPE(LightingShadow);

    // back up last viewport
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);

    // get Z plane
    // TODO: Tweak this slightly
    glm::vec2 zPlane = wr->getZPlane();
    float zNear = zPlane.x;
    float zFar = zPlane.y;

    // Calculate shadow view matrix
    glm::vec3 position = wr->getCamera().getCameraPosition();
    glm::vec3 lightDir = this->sunDirection;
    glm::mat4 depthProjectionMatrix = glm::ortho<float>(-100, 100, -100, 100, zNear, zFar);

    glm::vec3 shadowPos = position;// + (lightDir * 20.f / 2.f);
    glm::mat4 depthViewMatrix = glm::lookAt(shadowPos, -lightDir, glm::vec3(0,1,0));

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

    if(ImGui::BeginTable("lights", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch)) {
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
            char labelDiff[32], labelSpec[32], labelPos[32], labelDir[32];
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
                    ImGui::DragFloat3("##diff", &light->diffuseColor.x, 0.01, -10, 10);
                    ImGui::TableNextColumn();
                    ImGui::DragFloat3("##spec", &light->specularColor.x, 0.01, -10, 10);

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
                    ImGui::DragFloat3("##diff", &light->diffuseColor.x, 0.01, -10, 10);
                    ImGui::TableNextColumn();
                    ImGui::DragFloat3("##spec", &light->specularColor.x, 0.01, -10, 10);

                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("-");

                    ImGui::TableNextColumn();
                    ImGui::DragFloat3("##dir", &dir->direction.x, 0.01);

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
                    ImGui::DragFloat3("##diff", &light->diffuseColor.x, 0.01, -10, 10);
                    ImGui::TableNextColumn();
                    ImGui::DragFloat3("##spec", &light->specularColor.x, 0.01, -10, 10);

                    ImGui::TableNextColumn();
                    ImGui::DragFloat3("##pos", &pt->position.x);

                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("-");

                    ImGui::TableNextColumn();
                    ImGui::Text("%.3g/%.3g", pt->linearAttenuation, pt->quadraticAttenuation);
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
