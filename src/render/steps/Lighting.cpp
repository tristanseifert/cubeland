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
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

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

static const gl::GLfloat kSkyboxVertices[] = {
    // Positions
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
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

    // set up test lights
    this->setUpTestLights();

    // set up skybox and sky stuff
    this->setUpSkybox();
    this->setUpSky();

    // Set up shadowing-related stuff
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
 * Initializes skybox-related structures.
 */
void Lighting::setUpSkybox() {
    using namespace gfx;

    // Compile skybox shader and set up some vertex data
    this->skyboxProgram = std::make_shared<ShaderProgram>("/lighting/skybox.vert", "/lighting/skybox.frag");
    this->skyboxProgram->link();

    vaoSkybox = std::make_shared<VertexArray>();
    vboSkybox = std::make_shared<Buffer>(Buffer::Array);

    vaoSkybox->bind();
    vboSkybox->bind();

    vboSkybox->bufferData(sizeof(kSkyboxVertices), (void *) kSkyboxVertices);
    // index for vertices of skybox
    vaoSkybox->registerVertexAttribPointer(0, 3, VertexArray::Float, 3 * sizeof(gl::GLfloat), 0);

    VertexArray::unbind();

    // load cubemap texture
    std::vector<std::string> faces;
    faces.push_back("/cube/potato/right.jpg");
    faces.push_back("/cube/potato/left.jpg");
    faces.push_back("/cube/potato/top.jpg");
    faces.push_back("/cube/potato/bottom.jpg");
    faces.push_back("/cube/potato/back.jpg");
    faces.push_back("/cube/potato/front.jpg");

    this->skyboxTexture = std::make_shared<TextureCube>(0);
    this->skyboxTexture->setDebugName("SkyCube");
    this->skyboxTexture->loadFromImages(faces, true);
    TextureCube::unbind();
}

/**
 * Sets up the sky renderer.
 */
void Lighting::setUpSky() {
    using namespace gfx;

    // just compile our shader. vertices are squelched from there
    this->skyProgram = std::make_shared<ShaderProgram>("/lighting/sky.vert", "/lighting/sky.frag");
    this->skyProgram->link();

}

/**
 * Sets up the default lights.
 */
void Lighting::setUpTestLights(void) {
    // emulate sun
    this->sun = std::make_shared<gfx::DirectionalLight>();
    this->sun->setDirection(glm::vec3(1, 0, 0));
    this->sun->setColor(glm::vec3(1, 1, 1));

    this->addLight(this->sun);
}

/**
 * Tears down any resources we need to.
 */
Lighting::~Lighting() {

}



/**
 * Draws the debug view if enabled.
 */
void Lighting::startOfFrame() {
    if(this->showDebugWindow) {
        this->drawDebugWindow();
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

    // set viewport
    gl::glViewport(0, 0, this->viewportSize.x, this->viewportSize.y);

    // update sun angle based on time-of-day
    const auto time = renderer->getTime();
    glm::vec3 sunDir(0, sin(time * M_PI_2), cos(time * M_PI_2));
    this->sun->setDirection(sunDir);

    // use our lighting shader, bind textures and set their locations
    this->program->bind();

    this->gNormal->bind();
    this->gDiffuse->bind();
    this->gMatProps->bind();
    this->gDepth->bind();
    this->shadowTex->bind();

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

    // render the skybox and sky
    if(this->skyboxEnabled) {
        this->renderSkybox();
    }
    if(this->skyEnabled) {
        this->renderSky(renderer);
    }
}

/**
 * Renders the skybox.
 */
void Lighting::renderSkybox(void) {
    using namespace gfx;
    using namespace gl;
    PROFILE_SCOPE(Skybox);

    // re-enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    this->skyboxProgram->bind();

    // calculate a new view matrix with translation components removed
    glm::mat4 newView = glm::mat4(glm::mat3(this->viewMatrix));

    this->skyboxProgram->setUniformMatrix("view", newView);
    this->skyboxProgram->setUniformMatrix("projection", this->projectionMatrix);

    // bind VAO, texture, then draw
    this->vaoSkybox->bind();

    this->skyboxTexture->bind();
    this->skyboxProgram->setUniform1i("skyboxTex", this->skyboxTexture->unit);

    glDrawArrays(GL_TRIANGLES, 0, 36);
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
    this->skyProgram->setUniformVec("sunPosition", this->sun->getDirection());
    this->skyProgram->setUniform1f("cirrus", this->skyCloudCirrus);
    this->skyProgram->setUniform1f("cumulus", this->skyCloudCumulus);
    this->skyProgram->setUniform1i("numCumulus", this->skyCumulusLayers);

    // draw a full screen quad, WITH depth testing, behind everything else
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // glDepthFunc(GL_LESS);

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
    glm::vec3 lightDir = this->sun->getDirection();
    glm::mat4 depthProjectionMatrix = glm::ortho<float>(-100, 100, -100, 100, zNear, zFar);

    glm::vec3 shadowPos = position + (lightDir * 20.f / 2.f);
    glm::mat4 depthViewMatrix = glm::lookAt(shadowPos, -lightDir, glm::vec3(0,1,0));

    glm::mat4 lightSpaceMatrix = depthProjectionMatrix * depthViewMatrix;
    this->shadowViewMatrix = lightSpaceMatrix;

    // set viewport and a few other GL properties
    this->shadowFbo->bindRW();

    glViewport(0, 0, this->shadowW, this->shadowH);

    // glClearColor(0.f, 0.f, 0.f, 1.f);
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

    ImGui::Checkbox("Enabled##sky", &this->skyEnabled);

    ImGui::DragFloat("Cirrus Density", &this->skyCloudCirrus, 0.01, 0, 1);
    ImGui::DragFloat("Cumulus Density", &this->skyCloudCumulus, 0.01, 0, 1);
    ImGui::DragInt("Cumulus Layers", &this->skyCumulusLayers, 1, 1);

    // skybox
    ImGui::Text("Skybox");
    ImGui::Separator();

    ImGui::Checkbox("Enabled##skybox", &this->skyboxEnabled);

    // fog section
    ImGui::Text("Fog");
    ImGui::Separator();

    ImGui::DragFloat("Density", &this->fogDensity, 0.02, 0);
    ImGui::DragFloat("Offset", &this->fogOffset, 0.1, 0);

    ImGui::PopItemWidth();
    ImGui::ColorEdit3("Color", &this->fogColor.x);

    // shadow section
    ImGui::Text("Shadows");
    ImGui::Separator();

    ImGui::PushItemWidth(74);
    ImGui::DragFloat("Shadow Dir Coefficient", &this->shadowDirectionCoefficient, 0.02, 0.1, 6);
    
    ImGui::DragFloat("Shadow Coefficient", &this->shadowFactor, 0.02, 0, 1);

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
