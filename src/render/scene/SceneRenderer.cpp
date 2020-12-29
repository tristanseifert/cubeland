#include "SceneRenderer.h"
#include "Drawable.h"
#include "ChunkLoader.h"

#include "world/block/BlockRegistry.h"
#include "world/tick/TickHandler.h"

#include "render/WorldRenderer.h"
#include "render/chunk/WorldChunk.h"
#include "render/chunk/ChunkWorker.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/model/RenderProgram.h"

#include <Logging.h>
#include "io/Format.h"
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
    world::TickHandler::init();
    world::BlockRegistry::init();
    world::RegisterBuiltinBlocks();
    chunk::ChunkWorker::init();

    // set up the shaders for the color and shadow programs
    this->colorPrograms[kProgramChunkDraw] = WorldChunk::getProgram();
    this->colorPrograms[kProgramChunkHighlight] = WorldChunk::getHighlightProgram();

    this->shadowPrograms[kProgramChunkDraw] = WorldChunk::getShadowProgram();

    // create the chunk loader
    this->chunkLoader = new ChunkLoader;
}

/**
 * Release a bunch of global state
 */
SceneRenderer::~SceneRenderer() {
    // this will wait for the work queue to drain
    chunk::ChunkWorker::shutdown();

    // destroy all of our helper objects
    delete this->chunkLoader;

    // shut down other systems of the engine
    world::BlockRegistry::shutdown();
    world::TickHandler::shutdown();
}

/**
 * Invoke the start-of-frame handler on all drawables.
 */
void SceneRenderer::startOfFrame() {
    world::TickHandler::startOfFrame();

    this->chunkLoader->startOfFrame();

    this->projView = this->projectionMatrix * this->viewMatrix;
    this->chunkLoader->updateChunks(this->viewPosition, this->viewDirection, this->projView);
}

/**
 * Set up for rendering.
 */
void SceneRenderer::preRender(WorldRenderer *world) {
    using namespace gl;

    // set FoV on chunk loader
    this->chunkLoader->setFoV(world->getFoV());

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
    this->render(this->projView, this->viewDirection, false, true);

    // draw the highlights
    auto program = this->getProgram(kProgramChunkHighlight, false);
    program->bind();
    program->setUniformMatrix("projectionView", projView);

    this->chunkLoader->drawHighlights(program, projView);
}

/**
 * Performs the actual rendering of the scene.
 */
void SceneRenderer::render(const glm::mat4 &projView, const glm::vec3 &viewDir, const bool shadow, bool hasNormalMatrix) {
    using namespace gl;
    PROFILE_SCOPE(SceneRender);

    // draw chunks
    {
        auto program = this->getProgram(kProgramChunkDraw, shadow);
        program->bind();
        program->setUniformMatrix("projectionView", projView);

        this->chunkLoader->draw(program, projView, viewDir);
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


/**
 * Returns the position of the selected block, if there is one.
 */
std::optional<std::pair<glm::ivec3, glm::ivec3>> SceneRenderer::getSelectedBlockPos() const {
    if(!this->chunkLoader->lookAtBlock.has_value()) return std::nullopt;
    else if(!this->chunkLoader->lookAtBlockRelative.has_value()) return std::nullopt;

    return std::make_pair(*this->chunkLoader->lookAtBlock, *this->chunkLoader->lookAtBlockRelative);
}

/**
 * Returns a reference to the given chunk.
 */
std::shared_ptr<world::Chunk> SceneRenderer::getChunk(const glm::ivec2 &pos) {
    if(this->chunkLoader->loadedChunks.contains(pos)) {
        return this->chunkLoader->loadedChunks[pos];
    }

    // chunk not available
    return nullptr;
}

/**
 * Forces the selection to be recalculated next frame. This is useful after modifying the blocks
 * on screen.
 */
void SceneRenderer::forceSelectionUpdate() {
    this->chunkLoader->forceLookAtUpdate = true;
}

/**
 * Updates the color of the current selection.
 *
 * @note This will not apply to the next new selection, e.g. when the user moves.
 */
void SceneRenderer::setSelectionColor(const glm::vec4 &color) {
    // try to get the chunk
    auto pos = this->chunkLoader->lookAtSelectionMarker->first;
    if(!this->chunkLoader->chunks.contains(pos)) {
        return;
    }

    auto info = this->chunkLoader->chunks[pos];
    auto chunk = info.wc;
    if(!chunk) return;

    chunk->setHighlightColor(this->chunkLoader->lookAtSelectionMarker->second, color);
}

/**
 * Returns the most recent camera position.
 */
const glm::vec3 SceneRenderer::getCameraPos() const {
    return this->chunkLoader->lastPos;
}

/**
 * Sets the world source used to render world data.
 *
 * This should be called once, immediately before we render for the first time, to set the world
 * data. After, it should not be modified or changed; the behavior is undefined if this is done.
 */
void SceneRenderer::setWorldSource(std::shared_ptr<world::WorldSource> &source) {
    this->chunkLoader->setSource(source);
}
