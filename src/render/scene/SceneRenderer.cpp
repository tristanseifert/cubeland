#include "SceneRenderer.h"
#include "Drawable.h"

#include "render/chunk/WorldChunk.h"
#include "render/chunk/ChunkWorker.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/model/RenderProgram.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>
#include <mutils/time/profiler.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace render;

/**
 * Init; this loads the program/shader we use normally for drawing
 */
SceneRenderer::SceneRenderer() {
    // force initialization of some stuff
    chunk::ChunkWorker::init();

    // set up the shaders for the color and shadow programs
    this->colorPrograms[kProgramChunkDraw] = WorldChunk::getProgram();
    this->colorPrograms[kProgramChunkHighlight] = WorldChunk::getHighlightProgram();

    this->shadowPrograms[kProgramChunkDraw] = WorldChunk::getShadowProgram();

    // load the model
    auto chonker = std::make_shared<WorldChunk>();
    this->chunks.push_back(chonker);
}

/**
 * Release a bunch of global state
 */
SceneRenderer::~SceneRenderer() {
    chunk::ChunkWorker::shutdown();
}

/**
 * Invoke the start-of-frame handler on all drawables.
 */
void SceneRenderer::startOfFrame() {
    for(auto chunk : this->chunks) {
        chunk->frameBegin();
    }
}

/**
 * Set up for rendering.
 */
void SceneRenderer::preRender(WorldRenderer *) {
    using namespace gl;

    // set clear colour and depth testing; clear stencil as well
    // TODO: better granularity on stencil testing?
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT/* | GL_STENCIL_BUFFER_BIT*/);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // set up culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
}

/**
 * Actually renders the scene. This is called with the G-buffer attached.
 */
void SceneRenderer::render(WorldRenderer *renderer) {
    gl::glViewport(0, 0, this->viewportSize.x, this->viewportSize.y);

    glm::mat4 projView = this->projectionMatrix * this->viewMatrix;
    this->render(projView, false, true);
}

/**
 * Performs the actual rendering of the scene.
 */
void SceneRenderer::render(glm::mat4 projView, const bool shadow, bool hasNormalMatrix) {
    using namespace gl;
    PROFILE_SCOPE(SceneRender);

    // draw chunks
    {
        // set up the block rendering shader
        auto program = this->getProgram(kProgramChunkDraw, shadow);
        program->bind();
        program->setUniformMatrix("projectionView", projView);

        for(auto chunk : this->chunks) {
            this->prepareChunk(program, chunk, hasNormalMatrix);
            chunk->draw(program);
        }
    }
}

/**
 * Prepares a chunk for drawing.
 */
void SceneRenderer::prepareChunk(std::shared_ptr<gfx::RenderProgram> program,
        std::shared_ptr<WorldChunk> chunk, bool hasNormal) {
    // TODO: per chunk model matrix
    glm::mat4 model(1);

    // model = glm::rotate(model, this->time, glm::vec3(0, 1, 0));

    program->setUniformMatrix("model", model);

    if(hasNormal) {
        glm::mat3 normalMatrix;
        normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
        program->setUniformMatrix("normalMatrix", normalMatrix);
    }
}

/**
 * Cleans up some state after rendering.
 */
void SceneRenderer::postRender(WorldRenderer *) {
    using namespace gl;

    // disable culling again
    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);
}
