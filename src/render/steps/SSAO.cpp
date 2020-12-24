#include "SSAO.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/FrameBuffer.h"
#include "gfx/gl/buffer/VertexArray.h"

#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <imgui.h>

#include <cmath>
#include <vector>
#include <random>

using namespace render;

// vertices for a full-screen quad
static const gl::GLfloat kQuadVertices[] = {
    -1.0f,  1.0f, 0.0f,         0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,         0.0f, 0.0f,
     1.0f,  1.0f, 0.0f,         1.0f, 1.0f,
     1.0f, -1.0f, 0.0f,         1.0f, 0.0f,
};

/**
 * Creates the SSAO renderer.
 */
SSAO::SSAO() {
    // create a vertex array and buffer for the full screen quad data
    this->initQuadBuf();

    // framebuffers and blur textures
    this->initOcclusionBuf();
    this->initOcclusionBlurBuf();

    // generate the kernel and noise texture
    this->generateKernel();
    this->initNoiseTex();

    // shaders
    this->loadOcclusionShader();
    this->loadOcclusionBlurShader();

    // XXX: testing
    this->showDebugWindow = true;
}

/**
 * Creates a vertex array for rendering a full screen quad.
 */
void SSAO::initQuadBuf() {
    using namespace gfx;

    // set up a VAO and VBO for the full-screen quad
    this->vao = new VertexArray();
    this->vbo = new Buffer(Buffer::Array, Buffer::StaticDraw);

    this->vao->bind();
    this->vbo->bind();

    this->vbo->bufferData(sizeof(kQuadVertices), (void *) &kQuadVertices);

    // index of vertex position
    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, 5 * sizeof(gl::GLfloat), 0);
    // index of texture sampling position
    this->vao->registerVertexAttribPointer(1, 2, VertexArray::Float, 5 * sizeof(gl::GLfloat), 3 * sizeof(gl::GLfloat));

    VertexArray::unbind();
}

/**
 * Initializes the occlusion framebuffer and texture.
 */
void SSAO::initOcclusionBuf() {
    using namespace gfx;

    // set up the framebuffer and bind
    this->occlusionFb = new FrameBuffer();
    this->occlusionFb->bindRW();

    // allocate the texture
    this->occlusionTex = new Texture2D(5);
    this->occlusionTex->allocateBlank(1024, 768, Texture2D::RED16F);
    this->occlusionTex->setUsesLinearFiltering(false);
    this->occlusionTex->setDebugName("SsaoOcclusion");

    // attach texture to framebuffer and set them
    this->occlusionFb->attachTexture2D(this->occlusionTex, FrameBuffer::ColourAttachment0);
    
    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::End
    };
    this->occlusionFb->setDrawBuffers(buffers);

    // ensure buffer is ready
    XASSERT(FrameBuffer::isComplete(), "SSAO occlusion FBO incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Initializes the occlusion blur buffer.
 */
void SSAO::initOcclusionBlurBuf() {
    using namespace gfx;

    // set up the framebuffer and bind
    this->occlusionBlurFb = new FrameBuffer();
    this->occlusionBlurFb->bindRW();

    // allocate the texture
    this->occlusionBlurTex = new Texture2D(6);
    this->occlusionBlurTex->allocateBlank(1024, 768, Texture2D::RED16F);
    this->occlusionBlurTex->setUsesLinearFiltering(false);
    this->occlusionBlurTex->setDebugName("SsaoOcclusionBlur");

    // attach texture to framebuffer and set them
    this->occlusionBlurFb->attachTexture2D(this->occlusionBlurTex, FrameBuffer::ColourAttachment0);

    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::End
    };
    this->occlusionBlurFb->setDrawBuffers(buffers);

    // ensure buffer is ready
    XASSERT(FrameBuffer::isComplete(), "SSAO occlusion blur FBO incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Initializes the noise texture and fills it wit hthe initial noise value.
 */
void SSAO::initNoiseTex() {
    using namespace gfx;

    // allocate texture (TODO: 3 component texture hopefully works)
    this->noiseTex = new Texture2D(7);
    this->noiseTex->allocateBlank(4, 4, Texture2D::RGB16F);
    this->noiseTex->setUsesLinearFiltering(false);
    this->noiseTex->setWrapMode(Texture2D::Repeat, Texture2D::Repeat);

    // set up random generator
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;

    // build noise map
    std::vector<glm::vec3> ssaoNoise;
    for(size_t i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            0.0f); 
        ssaoNoise.push_back(noise);
    }

    // upload
    this->noiseTex->bufferSubData(4, 4, 0, 0,  Texture2D::RGB16F, ssaoNoise.data());
}

/**
 * Generates the SSAO sampling kernel.
 */
void SSAO::generateKernel(size_t size) {
    this->kernel.clear();
    this->kernel.reserve(size);

    // set up random generator
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;

    for(size_t i = 0; i < size; ++i) {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / ((float) size);

        // scale samples s.t. they're more aligned to center of kernel
        // scale = lerp(0.1f, 1.0f, scale * scale);
        scale = std::lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        this->kernel.push_back(sample);
    }
}

/**
 * Loads the occlusion drawing shader.
 */
void SSAO::loadOcclusionShader() {
    using namespace gfx;

    // load the shaderino
    this->occlusionShader = new ShaderProgram("/ssao/occlusion.vert", "/ssao/occlusion.frag");
    this->occlusionShader->link();

    // send the kernel to the shader
    this->occlusionShader->bind();

    this->occlusionShader->setUniform1i("texNoise", this->noiseTex->unit);
    this->sendKernel(this->occlusionShader);
}

/**
 * Loads the occlusion blur shader.
 */
void SSAO::loadOcclusionBlurShader() {
    using namespace gfx;

    // load the shaderino
    this->occlusionBlurShader = new ShaderProgram("/ssao/occlusion.vert", "/ssao/blur.frag");
    this->occlusionBlurShader->link();

    // send the unit to which the input texture is bound
    this->occlusionBlurShader->bind();
    this->occlusionBlurShader->setUniform1i("occlusion", this->occlusionTex->unit);
}

/**
 * Sends the current SSAO kernel to the specified shader.
 */
void SSAO::sendKernel(gfx::ShaderProgram *program) {
    for(size_t i = 0; i < this->kernel.size(); i++) {
        program->setUniformVec(f("samples[{}]", i), this->kernel[i]);
    }
}


/**
 * Yeets all the SSAO textures
 */
SSAO::~SSAO() {
    // delete the output textures and frame buffers
    delete this->occlusionFb;
    delete this->occlusionBlurFb;

    delete this->occlusionTex;
    delete this->occlusionBlurTex;

    // stuff for rendering SSAO
    delete this->noiseTex;

    delete this->occlusionShader;
    delete this->occlusionBlurShader;

    delete this->vao;
    delete this->vbo;
}


/**
 * Reallocates the output textures when the window is resized.
 */
void SSAO::reshape(int width, int height) {
    this->occlusionTex->allocateBlank(width, height, gfx::Texture2D::RED16F);
    this->occlusionBlurTex->allocateBlank(width, height, gfx::Texture2D::RED16F);

    // send new size to the occlusion shader
    this->occlusionShader->bind();

    glm::vec2 noiseScale(((float) width) / 4., ((float) height) / 4.);
    this->occlusionShader->setUniformVec("noiseScale", noiseScale);
}



/**
 * Start of frame handler, mostly for drawing debug window
 */
void SSAO::startOfFrame() {
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
}

/**
 * Prepares the rendering context.
 */
void SSAO::preRender(WorldRenderer *) {
    using namespace gl;

    glClearColor(1, 0, 0, 0);
}

/**
 * Performs drawing of the SSAO step. This is split into two stages; first, rendering the occlusion values, then
 * blurring the result.
 */
void SSAO::render(WorldRenderer *renderer) {
    PROFILE_SCOPE(SSAORender);

    using namespace gl;

    // we can use the same full screen quad VAO for everything
    this->vao->bind();

    glm::mat4 viewMatrixInv = glm::inverse(this->viewMatrix);
    glm::mat4 projMatrixInv = glm::inverse(this->projectionMatrix);

    // first step; render the occlusion value
    this->occlusionShader->bind();
    this->occlusionShader->setUniformMatrix("projection", this->projectionMatrix);
    this->occlusionShader->setUniformMatrix("viewMatrixInv", viewMatrixInv);
    this->occlusionShader->setUniformMatrix("projMatrixInv", projMatrixInv);

    if(this->needsParamUpdate) {
        this->occlusionShader->setUniform1i("kernelSize", this->ssaoKernelSize);
        this->occlusionShader->setUniform1f("radius", this->ssaoRadius);
        this->occlusionShader->setUniform1f("bias", this->ssaoBias);

        this->needsParamUpdate = false;
    }
    if(this->needsKernelUpdate) {
        this->sendKernel(this->occlusionShader);
        this->needsKernelUpdate = false;
    }

    this->occlusionFb->bindW();

    glClear(GL_COLOR_BUFFER_BIT);

    if(enabled) {
        this->gNormal->bind();
        this->gDepth->bind();
        this->noiseTex->bind();
        this->occlusionShader->setUniform1i("gNormal", this->gNormal->unit);
        this->occlusionShader->setUniform1i("gDepth", this->gDepth->unit);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    gfx::FrameBuffer::unbindRW();

    // perform the blurring
    this->occlusionBlurShader->bind();

    this->occlusionTex->bind();

    this->occlusionBlurFb->bindW();

    if(enabled) {
        gl::glDrawArrays(gl::GL_TRIANGLE_STRIP, 0, 4);
    } else {
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // unbind everything
    gfx::FrameBuffer::unbindRW();
    gfx::VertexArray::unbind();
}

/**
 * Draws the SSAO debug window
 */
void SSAO::drawDebugWindow() {
    // begin window
    if(!ImGui::Begin("SSAO Renderer", &this->showDebugWindow)) {
        return;
    }

    ImGui::Checkbox("Enabled", &this->enabled);

    // SSAO factors
    ImGui::PushItemWidth(74);

    if(ImGui::DragInt("Kernel Size", &this->ssaoKernelSize, 1, 1, 64)) {
        // TODO: regenerate kernel
        this->generateKernel(this->ssaoKernelSize);

        this->needsKernelUpdate = true;
        this->needsParamUpdate = true;
    }

    if(ImGui::DragFloat("Radius", &this->ssaoRadius, 0.001, 0.001)) {
        this->needsParamUpdate = true;
    }
    if(ImGui::DragFloat("Bias", &this->ssaoBias, 0.001)) {
        this->needsParamUpdate = true;
    }

    ImGui::PopItemWidth();

    // end
    ImGui::End();
}
