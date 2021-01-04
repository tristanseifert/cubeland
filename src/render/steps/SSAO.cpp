#include "SSAO.h"
#include "render/WorldRenderer.h"

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
SSAO::SSAO() : RenderStep("Render Debug", "SSAO") {
    // create a vertex array and buffer for the full screen quad data
    this->initQuadBuf();

    // framebuffers and blur textures
    this->initOcclusionBuf();

    // generate the kernel and noise texture
    this->generateKernel(this->ssaoKernelSize);
    this->initNoiseTex();

    // shaders
    this->loadOcclusionShader();

    // XXX: testing
    // this->showDebugWindow = true;
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

    this->needsKernelUpdate = true;
}

/**
 * Loads the occlusion drawing shader.
 */
void SSAO::loadOcclusionShader() {
    using namespace gfx;

    // load the shaderino
    this->occlusionShader = new ShaderProgram("ssao/occlusion.vert", "ssao/occlusion.frag");
    this->occlusionShader->link();

    // send the kernel to the shader
    this->occlusionShader->bind();

    this->occlusionShader->setUniform1i("texNoise", this->noiseTex->unit);
    this->sendKernel(this->occlusionShader);
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
    delete this->occlusionTex;

    // stuff for rendering SSAO
    delete this->noiseTex;

    delete this->occlusionShader;

    delete this->vao;
    delete this->vbo;
}


/**
 * Reallocates the output textures when the window is resized.
 */
void SSAO::reshape(int width, int height) {
    this->occlusionTex->allocateBlank(width, height, gfx::Texture2D::RED16F);

    // send new size to the occlusion shader
    this->occlusionShader->bind();

    glm::vec2 noiseScale(((float) width) / 4., ((float) height) / 4.);
    this->occlusionShader->setUniformVec("noiseScale", noiseScale);

    // set size
    this->occlusionSize = glm::vec2(width, height);
}



/**
 * Start of frame handler, mostly for drawing debug window
 */
void SSAO::startOfFrame() {
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
    if(this->showSsaoPreview) {
        this->drawSsaoPreview();
    }
}

/**
 * Prepares the rendering context.
 */
void SSAO::preRender(WorldRenderer *) {
    using namespace gl;

    if(this->enabled) {
        glClearColor(0, 0, 0, 0);
    } else {
        glClearColor(1, 0, 0, 0);
    }
    glDisable(GL_DEPTH_TEST);
}

/**
 * Restores depth testing after we've finished rendering.
 */
void SSAO::postRender(WorldRenderer *) {
    using namespace gl;

    glEnable(GL_DEPTH_TEST);
}

/**
 * Performs drawing of the SSAO step. This is split into two stages; first, rendering the occlusion values, then
 * blurring the result.
 */
void SSAO::render(WorldRenderer *renderer) {
    PROFILE_SCOPE(SSAO);

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

    this->occlusionShader->setUniform1f("thfov", tan(glm::radians(renderer->getFoV()) / 2.));
    this->occlusionShader->setUniform1f("aspect", this->occlusionSize.x / this->occlusionSize.y);

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

    if(this->enabled) {
        this->gNormal->bind();
        this->gDepth->bind();
        this->noiseTex->bind();
        this->occlusionShader->setUniform1i("gNormal", this->gNormal->unit);
        this->occlusionShader->setUniform1i("gDepth", this->gDepth->unit);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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
        ImGui::End();
        return;
    }

    if(ImGui::Button("Show Occlusion Buffer")) {
        this->showSsaoPreview = true;
    }

    ImGui::Checkbox("Enabled", &this->enabled);

    // SSAO factors
    ImGui::PushItemWidth(74);

    if(ImGui::DragInt("Kernel Size", &this->ssaoKernelSize, 1, 1, 64)) {
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

/**
 * Draw the SSAO preview window.
 *
 * It allows viewing the occlusion buffer texture.
 */
void SSAO::drawSsaoPreview() {
    static const char *kPreviewName[2] = {
        "Raw", "Blurred"
    };

    auto &io = ImGui::GetIO();

    // begin window
    if(!ImGui::Begin("SSAO Buffer", &this->showSsaoPreview, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // tint color
    ImGui::PushItemWidth(200);
    ImGui::ColorEdit4("Tint", &this->ssaoPreviewTint.x);
    ImGui::PopItemWidth();

    // index
    ImGui::PushItemWidth(74);
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10,0));
    ImGui::SameLine();

    if(ImGui::BeginCombo("Buffer", kPreviewName[this->previewTextureIdx])) {
        for(size_t j = 0; j < 1; j++) {
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
    ImVec2 imageSize(this->occlusionSize.x / 2 / io.DisplayFramebufferScale.x, 
            this->occlusionSize.y / 2 / io.DisplayFramebufferScale.y);
    ImVec4 tint(this->ssaoPreviewTint.x, this->ssaoPreviewTint.y, this->ssaoPreviewTint.z, this->ssaoPreviewTint.w);

    gl::GLuint textureId = this->occlusionTex->getGlObjectId();

    ImGui::Image((void *) (size_t) textureId, imageSize, uv0, uv1, tint, ImVec4(1,1,1,1));

    // end
    ImGui::End();
}
