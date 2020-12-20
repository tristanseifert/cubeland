#include "SceneRenderer.h"
#include "Drawable.h"
#include "ChunkLoader.h"

#include "world/block/BlockRegistry.h"
#include "world/WorldSource.h"
#include "world/FileWorldReader.h"
#include "world/generators/Terrain.h"

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
using namespace render::scene;

/**
 * Init; this loads the program/shader we use normally for drawing
 */
SceneRenderer::SceneRenderer() {
    // force initialization of some stuff
    world::BlockRegistry::init();
    chunk::ChunkWorker::init();

    // set up the shaders for the color and shadow programs
    this->colorPrograms[kProgramChunkDraw] = WorldChunk::getProgram();
    this->colorPrograms[kProgramChunkHighlight] = WorldChunk::getHighlightProgram();

    this->shadowPrograms[kProgramChunkDraw] = WorldChunk::getShadowProgram();

    // create the chunk loader
    this->chunkLoader = std::make_shared<ChunkLoader>();
}

/**
 * Release a bunch of global state
 */
SceneRenderer::~SceneRenderer() {
    // destroy all of our helper objects
    this->chunkLoader = nullptr;

    // shut down other systems of the engine
    chunk::ChunkWorker::shutdown();
    world::BlockRegistry::shutdown();
}

/**
 * Invoke the start-of-frame handler on all drawables.
 */
void SceneRenderer::startOfFrame() {
    this->chunkLoader->updateChunks(this->viewPosition);
}

/**
 * Set up for rendering.
 */
void SceneRenderer::preRender(WorldRenderer *) {
    using namespace gl;

    // set clear colour and depth testing; clear stencil as well
    // TODO: better granularity on stencil testing?
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // set up culling
    glEnable(GL_CULL_FACE);
}

/**
 * Actually renders the scene. This is called with the G-buffer attached.
 */
void SceneRenderer::render(WorldRenderer *renderer) {
    gl::glViewport(0, 0, this->viewportSize.x, this->viewportSize.y);

    glm::mat4 projView = this->projectionMatrix * this->viewMatrix;
    this->render(projView, false, true);

    // draw the highlights
    /*{
        PROFILE_SCOPE(ChunkHighlights);

        auto program = this->getProgram(kProgramChunkHighlight, false);
        program->bind();
        program->setUniformMatrix("projectionView", projView);

        for(auto &chunk : this->chunks) {
            if(chunk->needsDrawHighlights()) {
                this->prepareChunk(program, chunk, false);
                chunk->drawHighlights(program);
            }
        }
    }*/
}

/**
 * Performs the actual rendering of the scene.
 */
void SceneRenderer::render(glm::mat4 projView, const bool shadow, bool hasNormalMatrix) {
    using namespace gl;
    PROFILE_SCOPE(SceneRender);

    // draw chunks
    {
        auto program = this->getProgram(kProgramChunkDraw, shadow);
        program->bind();
        program->setUniformMatrix("projectionView", projView);

        this->chunkLoader->draw(program);
    }
}

/**
 * Prepares a chunk for drawing.
 */
void SceneRenderer::prepareChunk(std::shared_ptr<gfx::RenderProgram> program,
        std::shared_ptr<WorldChunk> chunk, bool hasNormal) {
    // TODO: per chunk model matrix
    glm::mat4 model(1);

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
    glDisable(GL_CULL_FACE);
}
