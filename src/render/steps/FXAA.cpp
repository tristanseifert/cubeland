#include "FXAA.h"
#include "../WorldRenderer.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/FrameBuffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace gl;
using namespace render;

// vertices for a full-screen quad
static const gl::GLfloat vertices[] = {
    -1.0f,  1.0f, 0.0f,         0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,         0.0f, 0.0f,
     1.0f,  1.0f, 0.0f,         1.0f, 1.0f,
     1.0f, -1.0f, 0.0f,         1.0f, 0.0f,
};

/**
 * Sets up the FXAA renderer.
 */
FXAA::FXAA() {
    // set up a VAO and VBO for the full-screen quad
    this->quadVAO = std::make_shared<gfx::VertexArray>();
    this->quadVBO = std::make_shared<gfx::Buffer>(gfx::Buffer::Array, gfx::Buffer::StaticDraw);

    this->quadVAO->bind();
    this->quadVBO->bind();

    this->quadVBO->bufferData(sizeof(vertices), (void *) &vertices);

    this->quadVAO->registerVertexAttribPointer(0, 3, gfx::VertexArray::Float, 5 * sizeof(GLfloat), 0);
    this->quadVAO->registerVertexAttribPointer(1, 2, gfx::VertexArray::Float, 5 * sizeof(GLfloat), 
                                               3 * sizeof(GLfloat));

    gfx::VertexArray::unbind();

    // load shader
    this->program = std::make_shared<gfx::ShaderProgram>("output/fxaa.vert", "output/fxaa.frag");
    this->program->link();
    this->program->bind();

    // allocate the FBO
    this->inFBO = std::make_shared<gfx::FrameBuffer>();
    this->inFBO->bindRW();

    // input colour (RGBA) buffer
    this->inColor = std::make_shared<gfx::Texture2D>(2);
    this->inColor->allocateBlank(1024, 768, gfx::Texture2D::RGBA8); // TODO: proper size
    this->inColor->setDebugName("FXAAColorIn");

    this->inFBO->attachTexture2D(this->inColor, gfx::FrameBuffer::ColourAttachment0);

    // Specify the buffers used for rendering
    gfx::FrameBuffer::AttachmentType buffers[] = {
        gfx::FrameBuffer::ColourAttachment0,
        gfx::FrameBuffer::End
    };
    this->inFBO->setDrawBuffers(buffers);

    // Ensure completeness of the buffer.
    XASSERT(gfx::FrameBuffer::isComplete(), "FXAA input FBO incomplete");
    gfx::FrameBuffer::unbindRW();
}

/**
 * Releases FXAA sources.
 */
FXAA::~FXAA() {

}

/**
 * Sets up the state before rendering.
 */
void FXAA::preRender(void) {
    // disable depth testing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}


/**
 * Applies FXAA on the input data, rendering to whatever framebuffer is bound
 * currently - usually that's the screen.
 */
void FXAA::render(WorldRenderer *renderer) {
    // use our HDR shader and bind its textures
    this->program->bind();
    this->inColor->bind();

    this->quadVAO->bind();
    //this->quadVBO->bind();

    // send some program information
    this->program->setUniform1i("inSceneColours", this->inColor->unit);
    this->program->setUniform1f("gamma", this->gamma);

    // set the FXAA quality settings
    this->program->setUniform1f("fxaaSubpixelAliasing", this->fxaaSubpixelAliasing);
    this->program->setUniform1f("fxaaEdgeThreshold", this->fxaaEdgeThreshold);
    this->program->setUniform1f("fxaaEdgeThresholdMin", this->fxaaEdgeThresholdMin);
    this->program->setUniform1f("fxaaEdgeSharpness", this->fxaaEdgeSharpness);

    // render a full-screen quad
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/**
 * Restores some GL state after rendering.
 */
void FXAA::postRender(void) {

}

/**
 * Resizes any textures following a window dimension change.
 */
void FXAA::reshape(int w, int h) {
    // Send some screen dimension information to the shader
    float width = (float) w;
    float height = (float) h;

    this->program->bind();

    glm::vec2 rcpFrame = glm::vec2(1.f / width, 1.f / height);
    this->program->setUniformVec("rcpFrame", rcpFrame);

    glm::vec4 rcpFrameOpt = glm::vec4(-0.5 / width, -0.5 / height, 0.5 / width,  0.5 / height);
    this->program->setUniformVec("rcpFrameOpt", rcpFrameOpt);
    glm::vec4 rcpFrameOpt2 = glm::vec4(-2.0 / width, -2.0 / height, 2.0 / width,  2.0 / height);
    this->program->setUniformVec("rcpFrameOpt2", rcpFrameOpt2);

    // Re-allocate the input texture
    this->inColor->allocateBlank(width, height, gfx::Texture2D::RGBA8);
}
