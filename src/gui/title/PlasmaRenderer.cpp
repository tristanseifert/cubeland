#include "PlasmaRenderer.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/FrameBuffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/texture/Texture2D.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gui::title;

static const gl::GLfloat kQuadVertices[] = {
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
};
/**
 * Initializes the plasma renderer's resources.
 */
PlasmaRenderer::PlasmaRenderer(const glm::ivec2 &size, const size_t _blurPasses) :
    blurPasses(_blurPasses) {
    using namespace gfx;

    // load plasma shader
    this->program = new ShaderProgram("title/plasma.vert", "title/plasma.frag");
    this->program->link();

    // allocate the vertex buffer for the quad
    this->vao = new VertexArray;
    this->vertices = new Buffer(Buffer::Array, Buffer::StaticDraw);

    this->vao->bind();
    this->vertices->bind();

    this->vertices->bufferData(sizeof(kQuadVertices), (void *) &kQuadVertices);

    // index of vertex position
    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, 5 * sizeof(gl::GLfloat), 0);
    // index of texture sampling position
    this->vao->registerVertexAttribPointer(1, 2, VertexArray::Float, 5 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat));

    VertexArray::unbind();

    // set up stuff for blurring
    if(_blurPasses) {
        // blur program
        this->blurProgram = new ShaderProgram("title/plasma.vert", "title/plasma_blur.frag");
        this->blurProgram->link();

        // output texture and its framebuffer
        this->blurTex = new Texture2D(1);
        this->blurTex->setWrapMode(Texture2D::MirroredRepeat, Texture2D::MirroredRepeat);
        this->blurTex->setUsesLinearFiltering(true);
        this->blurTex->setDebugName("PlasmaBlurOut");
        this->blurTex->allocateBlank(size.x, size.y, gfx::Texture2D::RGB16F);

        this->blurFb = new FrameBuffer;
        this->blurFb->bindRW();
        this->blurFb->attachTexture2D(this->blurTex, FrameBuffer::ColourAttachment0);

        FrameBuffer::AttachmentType buffers[] = {
            FrameBuffer::ColourAttachment0,
            FrameBuffer::End
        };
        this->blurFb->setDrawBuffers(buffers);

        XASSERT(FrameBuffer::isComplete(), "Plasma output framebuffer incomplete");
        FrameBuffer::unbindRW();
    }

    // allocate output texture and bind it to the framebuffer
    this->outTex = new Texture2D(0);
    this->outTex->setWrapMode(Texture2D::MirroredRepeat, Texture2D::MirroredRepeat);
    this->outTex->setUsesLinearFiltering(true);
    this->outTex->setDebugName("PlasmaOut");

    this->resize(size);

    this->fb = new FrameBuffer;
    this->fb->bindRW();
    this->fb->attachTexture2D(this->outTex, FrameBuffer::ColourAttachment0);

    FrameBuffer::AttachmentType buffers[] = {
        FrameBuffer::ColourAttachment0,
        FrameBuffer::End
    };
    this->fb->setDrawBuffers(buffers);

    XASSERT(FrameBuffer::isComplete(), "Plasma output framebuffer incomplete");
    FrameBuffer::unbindRW();

}

/**
 * Cleans up all our OpenGL stuff.
 */
PlasmaRenderer::~PlasmaRenderer() {
    delete this->vertices;
    delete this->vao;
    delete this->program;
    delete this->fb;
    delete this->outTex;

    if(this->blurPasses) {
        delete this->blurProgram;
        delete this->blurFb;
        delete this->blurTex;
    }
}

/**
 * Allocates the output texture's data.
 */
void PlasmaRenderer::resize(const glm::ivec2 &size) {
    this->outTex->allocateBlank(size.x, size.y, gfx::Texture2D::RGB16F);
    if(this->blurPasses && this->blurTex) {
        this->blurTex->allocateBlank(size.x, size.y, gfx::Texture2D::RGB16F);
    }

    this->viewport = size;
}



/**
 * Draws the plasma effect.
 */
void PlasmaRenderer::draw(const double time) {
    using namespace gfx;
    using namespace gl;

    // set up viewport
    glViewport(0, 0, this->viewport.x, this->viewport.y);

    // bind program and update its state
    this->program->bind();
    this->program->setUniform1f("time", fmod(time, 12.*M_PI));
    this->program->setUniformVec("viewport", glm::vec2(12.));

    // draw the quad into the framebuffer
    this->fb->bindRW();

    this->vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // perform blurring if necessary
    if(this->blurPasses) {
        this->blurProgram->bind();
        this->blurProgram->setUniformVec("inTextureSz", this->viewport);

        for(size_t i = 0; i < this->blurPasses; i++) {
            // vertical pass (into the blur framebuffer)
            this->blurFb->bindRW();
            this->outTex->bind();
            this->blurProgram->setUniform1i("inTexture", this->outTex->unit);
            this->blurProgram->setUniformVec("direction", glm::vec2(1, 0));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // horizontal pass (into output framebuffer)
            this->fb->bindRW();
            this->blurTex->bind();
            this->blurProgram->setUniform1i("inTexture", this->blurTex->unit);
            this->blurProgram->setUniformVec("direction", glm::vec2(0, 1));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    // clean up
    FrameBuffer::unbindRW();
    VertexArray::unbind();
}
