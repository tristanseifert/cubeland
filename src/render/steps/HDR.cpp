#include "HDR.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/FrameBuffer.h"
#include "gfx/gl/buffer/VertexArray.h"

#include <Logging.h>

#include <mutils/time/profiler.h>
#include <imgui.h>
#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace render;

// vertices for a full-screen quad
static const gl::GLfloat kQuadVertices[] = {
    -1.0f,  1.0f, 0.0f,		0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,		0.0f, 0.0f,
     1.0f,  1.0f, 0.0f,		1.0f, 1.0f,
     1.0f, -1.0f, 0.0f,		1.0f, 0.0f,
};

/**
 * Sets up the HDR renderer.
 */
HDR::HDR() : RenderStep("Render Debug", "HDR") {
    using namespace gfx;

    // set up the framebuffers
    this->setUpInputBuffers();
    this->setUpBloom();
    this->setUpHDRLumaBuffers();
    this->setUpTonemap();

    // set up a VAO and VBO for the full-screen quad
    this->quadVAO = std::make_unique<VertexArray>();
    this->quadVBO = std::make_unique<Buffer>(Buffer::Array, Buffer::StaticDraw);

    this->quadVAO->bind();
    this->quadVBO->bind();

    this->quadVBO->bufferData(sizeof(kQuadVertices), (void *) &kQuadVertices);

    this->quadVAO->registerVertexAttribPointer(0, 3, VertexArray::Float, 5 * sizeof(gl::GLfloat), 0);
    this->quadVAO->registerVertexAttribPointer(1, 2, VertexArray::Float, 5 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat));

    VertexArray::unbind();
}

/**
 * Sets up the framebuffer into which the previous rendering stage will output.
 */
void HDR::setUpInputBuffers(void) {
    using namespace gfx;

    // Load the shader program
    this->inHdrProgram = std::make_shared<ShaderProgram>("/hdr/hdr.vert", "/hdr/hdr.frag");
    this->inHdrProgram->link();

    // allocate the FBO
    this->inFBO = std::make_shared<FrameBuffer>();
    this->inFBO->bindRW();

    // get size of the viewport
    unsigned int width = 1024;
    unsigned int height = 768;

    // colour (RGB) buffer (gets the full range of lighting values from scene)
    this->inColour = std::make_shared<Texture2D>(0);
    this->inColour->allocateBlank(width, height, Texture2D::RGB16F);
    this->inColour->setUsesLinearFiltering(true);
    this->inColour->setDebugName("HDRColorIn");

    this->inFBO->attachTexture2D(this->inColour, FrameBuffer::ColourAttachment0);

    // Specify the buffers used for rendering
    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::End
    };
    this->inFBO->setDrawBuffers(buffers);

    // Ensure completeness of the buffer.
    XASSERT(FrameBuffer::isComplete(), "HDR input FBO incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Sets up the framebuffer utilized for the initial bright fragment extraction
 * and luminance calculation.
 */
void HDR::setUpHDRLumaBuffers() {
    using namespace gfx;

    // get size of the viewport
    unsigned int width = 1024;
    unsigned int height = 768;

    // allocate the FBO
    this->hdrLumaFBO = std::make_shared<FrameBuffer>();
    this->hdrLumaFBO->bindRW();

    // attach the first bloom texture at attachment 0
    this->hdrLumaFBO->attachTexture2D(this->inBloom1, FrameBuffer::ColourAttachment0);

    // luma (R) buffer; gets luminance per pixel
    this->sceneLuma = std::make_shared<Texture2D>(1);
    this->sceneLuma->allocateBlank(width, height, Texture2D::RGBA8);
    this->sceneLuma->setUsesLinearFiltering(true);
    // this->sceneLuma = this->lumaHisto->createSharedTextureForCurrentContext();
    this->sceneLuma->setDebugName("HDRPerPixelLuma");

    this->hdrLumaFBO->attachTexture2D(this->sceneLuma, FrameBuffer::ColourAttachment1);

    // Specify the buffers used for rendering
    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::ColourAttachment1,
        FrameBuffer::End
    };
    this->hdrLumaFBO->setDrawBuffers(buffers);

    // Ensure completeness of the buffer.
    XASSERT(FrameBuffer::isComplete(), "HDR/luma FBO incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Sets up buffers for performing bloom calculations. This involves two full-
 * size color buffers to use as ping-pong blur buffers.
 *
 * The way blurring works is that one texture (initially, this is inBloom1) will
 * be bound and read from by the pixel shader, whose output is rendered (via a
 * framebuffer) into the other texture, inBloom2. This process is then repeated
 * several times, switching input and output buffers as needed until the blur
 * size is sufficient.
 */
void HDR::setUpBloom(void) {
    using namespace gfx;

    // Load the shader program
    this->bloomBlurProgram = std::make_shared<ShaderProgram>("/hdr/bloom.vert", "/hdr/bloom.frag");
    this->bloomBlurProgram->link();

    // get size of the viewport
    unsigned int vpWidth = 1024;
    unsigned int vpHeight = 768;

    unsigned int width = vpWidth / this->bloomTexDivisor;
    unsigned int height = vpHeight / this->bloomTexDivisor;

    // Allocate two bloom textures at full size
    this->inBloom1 = std::make_shared<Texture2D>(2);
    this->inBloom1->allocateBlank(width, height, Texture2D::RGB16F);
    this->inBloom1->setDebugName("HDRBloomBuf1");

    this->inBloom2 = std::make_shared<Texture2D>(2);
    this->inBloom2->allocateBlank(width, height, Texture2D::RGB16F);
    this->inBloom2->setDebugName("HDRBloomBuf2");

    // these are the buffers the blur buffers will utilize
    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::End
    };

    // Allocate the first bloom framebuffer (output to buf2)
    this->inFBOBloom1 = std::make_shared<FrameBuffer>();
    this->inFBOBloom1->bindRW();

    this->inFBOBloom1->attachTexture2D(this->inBloom2, FrameBuffer::ColourAttachment0);
    this->inFBOBloom1->setDrawBuffers(buffers);

    XASSERT(FrameBuffer::isComplete(), "Bloom FBO 1 incomplete");
    FrameBuffer::unbindRW();


    // Allocate the first bloom framebuffer (output to buf1)
    this->inFBOBloom2 = std::make_shared<FrameBuffer>();
    this->inFBOBloom2->bindRW();

    this->inFBOBloom2->attachTexture2D(this->inBloom1, FrameBuffer::ColourAttachment0);
    this->inFBOBloom2->setDrawBuffers(buffers);

    XASSERT(FrameBuffer::isComplete(), "Bloom FBO 2 incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Sets up the shader to perform the final tonemapping.
 */
void HDR::setUpTonemap(void) {
    // Load the shader program
    this->tonemapProgram = std::make_shared<gfx::ShaderProgram>("/hdr/tonemap.vert", "/hdr/tonemap.frag");
    this->tonemapProgram->link();
}



/**
 * Tears down the HDR renderer.
 */
HDR::~HDR() {

}



/**
 * Resize the HDR input and luminance textures
 */
void HDR::reshape(int width, int height) {
    using namespace gfx;

    // Re-allocate the input texture
    this->inColour->allocateBlank(width, height, Texture2D::RGB16F);
    this->sceneLuma->allocateBlank(width, height, Texture2D::RGBA8);

    // Re-allocate the bloom textures
    unsigned int bloomW = width / this->bloomTexDivisor;
    unsigned int bloomH = height / this->bloomTexDivisor;

    this->inBloom1->allocateBlank(bloomW, bloomH, Texture2D::RGB16F);
    this->inBloom2->allocateBlank(bloomW, bloomH, Texture2D::RGB16F);
    this->bloomBufferDirty = true;
}



/**
 * Prepares for rendering.
 */
void HDR::preRender(WorldRenderer *) {
    using namespace gl;

    // do not depth test fragments since we're drawing a full screen quad only
    glDisable(GL_DEPTH_TEST);

    // clear the output buffer
    // glClear(GL_COLOR_BUFFER_BIT);

    // perform a render step
    this->_exposureStep();
}

/*
 * Performs rendering of the HDR.
 */
void HDR::render(WorldRenderer *renderer) {
    if(this->bloomEnabled || this->bloomBufferDirty) {
        // scale viewport for the blur texture size
        unsigned int width = this->viewportSize.x / this->bloomTexDivisor;
        unsigned int height = this->viewportSize.y / this->bloomTexDivisor;

        gl::glViewport(0, 0, width, height);
    }

    // extract the bright/blur parts of the buffers
    if(this->bloomEnabled) {
        this->renderExtractBright();
        this->renderBlurBright();
    } else {
        // clear the bloom buffers if needed
        if(this->bloomBufferDirty) {
            gl::glClearColor(0, 0, 0, 0);

            this->inFBOBloom1->bindRW();
            gl::glClear(gl::GL_COLOR_BUFFER_BIT);

            this->inFBOBloom2->bindRW();
            gl::glClear(gl::GL_COLOR_BUFFER_BIT);

            gfx::FrameBuffer::unbindRW();
        }
    }
    
    // reset viewport
    if(this->bloomEnabled || this->bloomBufferDirty) {
        gl::glViewport(0, 0, this->viewportSize.x, this->viewportSize.y);
    }

    // perform tonemapping
    this->renderPerformTonemapping();
}

/**
 * Run the pixel shader that extracts the bright fragments.
 */
void HDR::renderExtractBright(void) {
    using namespace gl;
    PROFILE_SCOPE(HdrExtractBright);

    this->hdrLumaFBO->bindRW();

    // clear the output pls
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // use the "HDR" shader to get the bright areas to a separate buffer
    this->inHdrProgram->bind();
    this->inColour->bind();

    // set up the program's input buffers
    this->inHdrProgram->setUniform1i("texInColour", this->inColour->unit);
    this->inHdrProgram->setUniform1f("lumaThreshold", this->extractLumaThresh);

    // render a full-screen quad
    this->quadVAO->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // unbind framebuffers
    gfx::FrameBuffer::unbindRW();
}

/**
 * Blur the input bloom buffer, placing the result in the output buffer, then
 * swap buffers and repeat as many times as needed.
 */
void HDR::renderBlurBright(void) {
    using namespace gl;
    PROFILE_SCOPE(HdrBlur);

    this->bloomBufferDirty = true;

    // get size of the viewport
    unsigned int width = this->viewportSize.x / this->bloomTexDivisor;
    unsigned int height = this->viewportSize.y / this->bloomTexDivisor;

    // activate the bloom shader and transfer some settings
    this->bloomBlurProgram->bind();

    this->bloomBlurProgram->setUniformVec("resolution", glm::vec2(width, height));
    this->bloomBlurProgram->setUniform1i("blurKernelSz", this->blurSize);

    // bind VAO for a full-screen quad
    this->quadVAO->bind();

    // run as many times as requested
    for(int i = 0; i < (this->numBlurPasses * 2); i++) {
        PROFILE_SCOPE(HdrBlurPass);

        // first iteration does horizontal blurring
        if((i & 1) == 0) {
            this->inBloom1->bind();
            this->inFBOBloom1->bindRW();

            this->bloomBlurProgram->setUniform1i("inTex", this->inBloom1->unit);
            this->bloomBlurProgram->setUniformVec("direction", glm::vec2(1, 0));
        }
        // second iteration does vertical blurring
        else {
            this->inBloom2->bind();
            this->inFBOBloom2->bindRW();

            this->bloomBlurProgram->setUniform1i("inTex", this->inBloom2->unit);
            this->bloomBlurProgram->setUniformVec("direction", glm::vec2(0, 1));
        }

        // render a full-screen quad
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // unbind framebuffers
    gfx::FrameBuffer::unbindRW();
}

/**
 * Combine the HDR output with that of the bloom blur section, then execute
 * tonemapping and calculate the luminance of the tonemapped output for the
 * FXAA shader later on.
 */
void HDR::renderPerformTonemapping(void) {
    using namespace gl;
    PROFILE_SCOPE(HdrTonemapping);

    // Bind the output framebuffer
    this->outFBO->bindRW();

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind input textures
    this->inColour->bind();
    this->inBloom1->bind();

    // activate the bloom shader and transfer some settings
    this->tonemapProgram->bind();

    this->tonemapProgram->setUniform1i("inSceneColors", this->inColour->unit);
    this->tonemapProgram->setUniform1i("inBloomBlur", this->inBloom1->unit);

    this->tonemapProgram->setUniformVec("vignetteParams", this->vignetteParams);
    this->tonemapProgram->setUniformVec("hsvAdjust", this->hsvAdjust);
    this->tonemapProgram->setUniform1f("exposure", this->exposure);
    this->tonemapProgram->setUniform1f("bloomFactor", this->bloomEnabled ? this->bloomFactor : 0);

    // TODO: Dynamically calculate white point
    this->tonemapProgram->setUniformVec("whitePoint", this->whitePoint);

    this->tonemapProgram->setUniformVec("uchimura1", this->uchimura1);
    this->tonemapProgram->setUniformVec("uchimura2", this->uchimura2);

    // bind VAO for a full-screen quad and render
    this->quadVAO->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // unbind framebuffer
    gfx::FrameBuffer::unbindRW();
}

/**
 * Do nothing
 */
void HDR::postRender(WorldRenderer *) {
    // clear per-frame flags
    this->bloomBufferDirty = false;
}

/**
 * Perform the luma histogram calculation
 */
void HDR::startOfFrame(void) {
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
    if(this->showTexturePreview) {
        this->drawTexturePreview();
    }

    if((this->histoCounter++) == this->histoFrameWait) {
        // reset counter
        this->histoCounter = 0;
        // TODO: calculate histogram with callback
    }
}

/**
 * Binds the HDR input buffer.
 */
void HDR::bindHDRBuffer(void) {
    this->inFBO->bindRW();
}

/**
 * Unbinds the HDR input buffer./
 */
void HDR::unbindHDRBuffer(void) {
    gfx::FrameBuffer::unbindRW();
}

/**
 * Sets the depth texture.
 */
void HDR::setDepthBuffer(std::shared_ptr<gfx::Texture2D> depth) {
    using namespace gfx;

    // check if the texture changed
    if(this->inDepth == depth) {
        return;
    } else {
        if(this->inDepth != NULL) {
            this->inFBO->attachTexture2D(this->inDepth, FrameBuffer::DepthStencil);
            XASSERT(FrameBuffer::isComplete(), "HDR input FBO incomplete");
            return;
        }
    }

    this->inDepth = depth;

    // attach the texture
    this->inFBO->bindRW();
    this->inFBO->attachTexture2D(this->inDepth, FrameBuffer::DepthStencil);

    // Ensure completeness of the buffer.
    XASSERT(FrameBuffer::isComplete(), "HDR input FBO incomplete");
    FrameBuffer::unbindRW();
}

/**
 * Binds the luma texture to the output framebuffer.
 */
void HDR::setOutputFBO(std::shared_ptr<gfx::FrameBuffer> fbo, bool attach) {
    using namespace gfx;

    // just save the texture
    this->outFBO = fbo;

    // if attach is true, attach it as well
    if(attach == true) {
        // attach texture
        this->outFBO->bindRW();
        this->outFBO->attachTexture2D(this->sceneLuma, FrameBuffer::ColourAttachment1);

        // Specify the buffers used for rendering
        FrameBuffer::AttachmentType buffers[] = {
            FrameBuffer::ColourAttachment0,
            FrameBuffer::ColourAttachment1,
            FrameBuffer::End
        };
        this->outFBO->setDrawBuffers(buffers);

        // Ensure completeness of the buffer.
        XASSERT(FrameBuffer::isComplete(), "HDR output FBO incomplete");
        FrameBuffer::unbindRW();
    }
}





/**
 * Perform whatever step the exposure calculation deems neccesary.
 */
void HDR::_exposureStep() {
    return;

    // this is the coefficient to add to the exposure value
    double x = (this->exposureChangeTicks++) * this->exposureDeltaMultiplier;
    double expoDelta = (exp(x / 30.) - 1.) / 15.;

    // add it if the exposure is to change
    if(this->exposureDirection == UP) {
        this->exposure += expoDelta;
    } else if(this->exposureDirection == DOWN) {
        this->exposure -= expoDelta;
    }
    // If no exposure change is needed, reset tick counter
    else if(this->exposureDirection == NONE) {
        this->exposureChangeTicks = 0.f;
    }

    // clamp exposure to reasonable values
    this->exposure = fmax(this->exposure, 0.3f);
    this->exposure = fmin(this->exposure, 5.3f);
}



/**
 * Draws the HDR renderer debug window
 */
void HDR::drawDebugWindow() {
  // short circuit drawing if not visible
    if(!ImGui::Begin("HDR Renderer", &this->showDebugWindow)) {
        goto done;
    }

    if(ImGui::Button("Buffer Viewer")) {
        this->showTexturePreview = true;
    }

    // bloom
    ImGui::Text("Bloom");
    ImGui::Separator();

    ImGui::Checkbox("Enabled", &this->bloomEnabled);

    ImGui::PushItemWidth(74);
    ImGui::DragInt("Blur Passes", &this->numBlurPasses, 1, 3, 19);
    ImGui::DragFloat("Blend Factor", &this->bloomFactor, 0.01, 0, 2);
    ImGui::DragFloat("Luma Threshold", &this->extractLumaThresh, 0.01, 0, 2);

    if(ImGui::DragInt("Size Factor", &this->bloomTexDivisor, 1, 1, 16)) {
        unsigned int bloomW = this->viewportSize.x / this->bloomTexDivisor;
        unsigned int bloomH = this->viewportSize.y / this->bloomTexDivisor;

        this->inBloom1->allocateBlank(bloomW, bloomH, gfx::Texture2D::RGB16F);
        this->inBloom2->allocateBlank(bloomW, bloomH, gfx::Texture2D::RGB16F);
    }

    // exposure
    ImGui::Text("Tonemapping");
    ImGui::Separator();
    ImGui::DragFloat("Exposure", &this->exposure, 0.01, 0.1, 6);

    ImGui::DragFloat("Max Brightness", &this->uchimura1.x, 0.01, 0.1);
    ImGui::DragFloat("Contrast", &this->uchimura1.y, 0.01);
    ImGui::DragFloat("Linear Section Begin", &this->uchimura1.z, 0.01, 0);
    ImGui::DragFloat("Linear Section End", &this->uchimura2.x, 0.01, this->uchimura1.z);
    ImGui::DragFloat("Black", &this->uchimura2.y, 0.01, 0);
    ImGui::DragFloat("Pedestal", &this->uchimura2.z, 0.01, 0);

    ImGui::PopItemWidth();
    ImGui::PushItemWidth(150);
    ImGui::DragFloat3("White Point", &this->whitePoint.x, 0.01, 0);

    ImGui::DragFloat3("HSV Adjust", &this->hsvAdjust.x, 0.01, 0);
    ImGui::DragFloat2("Vignette", &this->vignetteParams.x, 0.01, 0);

    ImGui::PopItemWidth();

done:;
    ImGui::End();
}

/**
 * Draws the texture preview window.
 */
void HDR::drawTexturePreview() {
    static const char *kPreviewName[5] = {
        "In Color", "In Depth", "Bloom 1", "Bloom 2", "Scene Luma",
    };

    auto &io = ImGui::GetIO();
  // short circuit drawing if not visible
    if(!ImGui::Begin("HDR Buffers", &this->showTexturePreview, ImGuiWindowFlags_AlwaysAutoResize)) {
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
    ImGui::PushItemWidth(100);
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

    gl::GLuint textureId = this->inColour->getGlObjectId();
    if(this->previewTextureIdx == 1) {
        textureId = this->inDepth->getGlObjectId();
    } else if(this->previewTextureIdx == 2) {
        textureId = this->inBloom1->getGlObjectId();
        imageSize.x = imageSize.x / this->bloomTexDivisor;
        imageSize.y = imageSize.y / this->bloomTexDivisor;
    } else if(this->previewTextureIdx == 3) {
        textureId = this->inBloom2->getGlObjectId();
        imageSize.x = imageSize.x / this->bloomTexDivisor;
        imageSize.y = imageSize.y / this->bloomTexDivisor;
    } else if(this->previewTextureIdx == 4) {
        textureId = this->sceneLuma->getGlObjectId();
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
