#include "WorldChunk.h"
#include "WorldChunkDebugger.h"
#include "ChunkWorker.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <uuid.h>
#include <glbinding/gl/gl.h>
#include <glm/ext.hpp>

using namespace render;
using namespace render::chunk;

/**
 * Fixed vertices (x, y, z), normals (xyz) and (uv) for a cube that's one unit in each dimension.
 */
static const gl::GLfloat kCubeVertices[] = {
    // back face
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.0f, 0.0f,

    // front face
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    0.0f, 1.0f,

    // left face
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,

    // right face
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   1.0f, 0.0f,

     // bottom face
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,   0.0f, 0.0f,

    // top face
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   0.0f, 1.0f
};

/// render program (for forward rendering)
std::shared_ptr<gfx::RenderProgram> WorldChunk::getProgram() {
    auto p = std::make_shared<gfx::RenderProgram>("/model/chunk.vert", "/model/chunk.frag", true);
    p->link();
    return p;
}
/// render program for highlight rendering
std::shared_ptr<gfx::RenderProgram> WorldChunk::getHighlightProgram() {
    auto p = std::make_shared<gfx::RenderProgram>("/model/chunk_highlight.vert",
            "/model/chunk_highlight.frag", true);
    p->link();
    return p;
}
/// render program for shadow rendering
std::shared_ptr<gfx::RenderProgram> WorldChunk::getShadowProgram() {
    auto p = std::make_shared<gfx::RenderProgram>("/model/chunk_shadow.vert",
            "/model/chunk_shadow.frag", false);
    p->link();
    return p;
}

/**
 * Sets up the static buffers used to draw the blocks in the world.
 */
WorldChunk::WorldChunk() {
    using namespace gfx;

    // allocate memory
    this->exposureMap.resize((256 * 256 * 256));

    // create buffers and prepare to bind the vertex attrib object
    this->vao = std::make_shared<VertexArray>();

    this->vbo = std::make_shared<Buffer>(Buffer::Array, Buffer::StaticDraw);
    this->vbo->bind();
    this->vbo->bufferData(sizeof(kCubeVertices), (void *) &kCubeVertices);
    this->vbo->unbind();

    BlockInstanceData data;
    this->instanceBuf = std::make_shared<Buffer>(Buffer::Array, Buffer::DynamicDraw);
    this->instanceBuf->bind();
    this->instanceBuf->bufferData(sizeof(BlockInstanceData), (void *) &data);
    this->instanceBuf->unbind();


    // define the attribute layout for the fixed per-vertex buffer
    this->vao->bind();
    this->vbo->bind();

    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat), 
            0); // vertex position
    this->vao->registerVertexAttribPointer(1, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat)); // normals
    this->vao->registerVertexAttribPointer(2, 2, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            6 * sizeof(gl::GLfloat)); // texture coordinate

    // describe the attribute layout for indexed parameters
    this->instanceBuf->bind();

    const size_t instanceElementSize = 3 * sizeof(gl::GLfloat);
    this->vao->registerVertexAttribPointer(3, 3, VertexArray::Float, instanceElementSize, 0, 1); // per vertex position offset
    this->instanceBuf->unbind();

    VertexArray::unbind();

    this->initHighlightBuffer();

    // lastly, load the placeholder texture
    this->placeholderTex = std::make_shared<Texture2D>(6);
    this->placeholderTex->loadFromImage("/test/empty/whitegreen.png");
}

/**
 * If any of our buffers are stale, begin updating them in the background at the start of the frame
 * so that hopefully, by the time we need to go draw, they're done.
 * Perform some calculations of updating data in the background at the start of a frame.
 */
void WorldChunk::frameBegin() {
    // instance data is out of date
    if(this->instanceDataNeedsUpdate) {
        ChunkWorker::pushWork([&]() -> void {
            // clear flag first, so if data changes while we're updating, it's fixed next frame
            this->instanceDataNeedsUpdate = false;
            this->fillInstanceBuf();
        });
    }
    // highlight related stuff
    if(this->highlightsNeedUpdate) { // queue updating of buffer in background
        ChunkWorker::pushWork([&]() -> void {
            // clear flag first, so if data changes while we're updating, it's fixed next frame
            this->highlightsNeedUpdate = false;
            this->updateHighlightBuffer();
        });
    }

    // draw debugger
    if(this->debugger) {
        this->debugger->draw();
    }
}

/**
 * Uses instanced rendering to draw the blocks of the chunk.
 *
 * At this point, our draw list should have been culled to the point that only blocks exposed to
 * air (e.g. ones that could be visible) are in it.
 */
void WorldChunk::draw(std::shared_ptr<gfx::RenderProgram> program) {
    using namespace gl;

    // transfer any buffers that need it
    this->transferBuffers();

    // set up for rendering
    program->bind();
    if(program->rendersColor()) {
        this->placeholderTex->bind();
        program->setUniform1i("texture_diffuse1", this->placeholderTex->unit);
    }

    if(this->numInstances) {
        if(this->drawWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        this->vao->bind();
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, this->numInstances);
        gfx::VertexArray::unbind();

        if(this->drawWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    // if no chunk data available, draw a wireframe outline of the chunk
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        this->vao->bind();
        // glDrawArrays(GL_TRIANGLES, 0, 36);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, 1);
        gfx::VertexArray::unbind();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

/**
 * Transfers dirty buffers to the GPU.
 */
void WorldChunk::transferBuffers() {
    PROFILE_SCOPE(BufferXfer);

    // instance data buffer
    if(this->instanceBufDirty) {
        const auto size = sizeof(BlockInstanceData) * this->instanceData.size();
        if(size) {
            this->instanceBuf->bind();
            this->instanceBuf->bufferData(size, this->instanceData.data());
            this->instanceBuf->unbind();

            this->numInstances = this->instanceData.size();
        } else {
            Logging::warn("Chunk {} instance buffer is empty", (void *) this->chunk.get());
        }

        this->instanceBufDirty = false;
    }
}



/**
 * Sets the chunk that we're going to be rendering.
 *
 * This immediately kicks off (on the shared chunk worker thread pool) the buffer update
 * computations, since those can take a while. If we're still waiting on this when the draw call
 * comes around, we'll skip updating the buffer and possibly draw stale data.
 */
void WorldChunk::setChunk(std::shared_ptr<world::Chunk> chunk) {
    this->chunk = chunk;

    this->instanceDataNeedsUpdate = true;
    this->withoutCaching = true;
    this->exposureMapNeedsUpdate = true;
}

/**
 * Fills the instance buffer with info on each of the blocks to be drawn.
 *
 * If needed, the "exposed blocks" map is updated as well.
 */
void WorldChunk::fillInstanceBuf() {
    using namespace world;

    PROFILE_SCOPE(FillInstanceBuf);

    // counters
    size_t numCulled = 0, numTotal = 0;

    // clear caches if needed
    if(this->withoutCaching) {
        PROFILE_SCOPE(ClearCaches);

        this->instanceData.clear();
        this->exposureIdMaps.clear();
        std::fill(this->exposureMap.begin(), this->exposureMap.end(), false);
    }

    // update the exposure ID maps
    if(this->withoutCaching || this->exposureIdMaps.size() != this->chunk->sliceIdMaps.size()) {
        this->generateBlockIdMap();
        this->exposureMapNeedsUpdate = true;
    }

    // update exposed blocks map if chunk is dirty
    if(this->withoutCaching || this->exposureMapNeedsUpdate) {
        this->updateExposureMap();
        this->exposureMapNeedsUpdate = false;
    }

    // update the actual instance buffer itself
    for(size_t y = 0; y < Chunk::kMaxY; y++) {
        PROFILE_SCOPE(ProcessSlice);
        const size_t yOffset = (y & 0xFF) << 16;

        // if there's no blocks at this Y level, check the next one
        auto slice = this->chunk->slices[y];
        if(!slice) continue;

        // iterate over each of the slice's rows
        for(size_t z = 0; z < 256; z++) {
            // skip empty rows
            auto row = slice->rows[z];
            if(!row) continue;

            const size_t zOffset = yOffset |  ((z & 0xFF) << 8);

            // process each block in this row
            const auto &map = this->chunk->sliceIdMaps[row->typeMap];

            for(size_t x = 0; x < 256; x++) {
                // skip blocks to not draw (e.g. air) XXX: handle this properly
                uint8_t temp = row->at(x);
                if(temp == 0) continue;
                numTotal++;

                // skip block if not exposed
                if(!this->exposureMap[zOffset + x]) {
                    numCulled++;
                    continue;
                }

                /// TODO: properly drawing
                BlockInstanceData data;
                data.blockPos = glm::vec3(x, y, z);

                this->instanceData.push_back(std::move(data));
            }
        }
    }

    // ensure the buffer is transferred on the next frame
    Logging::trace("Filled {} items to instance buffer ({} total, culled {} blocks ({}%))",
            this->instanceData.size(), numTotal, numCulled, 
            100 * (((float) numCulled) / ((float) numTotal)));
    this->instanceBufDirty = true;
}

/**
 * Updates the map of what blocks are exposed.
 *
 * This works by generating a 3x256x256 boolean grid, indicating whether the block at that position
 * is air-like (for purposes of exposure calculations) or whether it's solid. The grids are
 * centered at the current Y position; that is to say, there will be one layer of this data for
 * both the immediately above and below of all positions.
 */
void WorldChunk::updateExposureMap() {
    PROFILE_SCOPE(UpdateExposureMap);

    // generate bitset data for Y=0 and Y=1
    std::bitset<256*256> above, current, below;

    below.reset();
    this->buildAirMap(this->chunk->slices[0], current);
    this->buildAirMap(this->chunk->slices[1], above);

    // iterate every row
    for(size_t y = 0; y < 256; y++) {
        // ignore empty slices
        const size_t yOff = ((y & 0xFF) << 16);
        auto slice = this->chunk->slices[y];
        if(!slice) {
            std::fill(this->exposureMap.begin() + yOff,
                      this->exposureMap.begin() + (yOff + 0x10000), false);
            goto nextRow;
        }

        // iterate over each row
        for(size_t z = 0; z < 256; z++) {
            // ignore empty rows
            const size_t zOff = yOff | ((z & 0xFF) << 8);
            auto row = slice->rows[z];

            if(!row) {
                std::fill(this->exposureMap.begin() + zOff, 
                          this->exposureMap.begin() + (zOff + 0x100), false);
                continue;
            }

            // iterate over all blocks in the row. check if any of its faces touch 'air'
            for(size_t x = 0; x < 256; x++) {
                const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);
                bool visible = false;

                // above or below
                if(above[airMapOff]) {
                    visible = true;
                    goto writeResult;
                }
                if(below[airMapOff]) {
                    visible = true;
                    goto writeResult;
                }

                // check adjacent faces
                for(int i = -1; i <= 1; i+=2) {
                    // left or right
                    if((x == 0 && i == -1) || (x == 255 && i == 1) || current[airMapOff + i]) {
                        visible = true;
                        goto writeResult;
                    }
                    // front or back
                    if((z == 0 && i == -1) || (z == 255 && i == 1) || current[airMapOff + (i * 256)]) {
                        visible = true;
                        goto writeResult;
                    }
                }

writeResult:;
                // write the result into the exposure map
                this->exposureMap[zOff + x] = visible;
            }
        }

        // generate air map for the next row
nextRow:;
        below = std::move(current);
        current = std::move(above);

        if((y+2) < this->chunk->slices.size()) {
            above.reset();
            this->buildAirMap(this->chunk->slices[y+2], above);
        } else {
            above.set();
        }
    }
}
/**
 * For a particular Y layer, generates a bitmap indicating whether the block at the given (Z, X)
 * position is air-like or not.
 *
 * Indices into the bitset are 16-bit 0xZZXX coordinates.
 */
void WorldChunk::buildAirMap(std::shared_ptr<world::ChunkSlice> slice, std::bitset<256*256> &b) {
    // if the slice is empty (e.g. nonexistent,) bail; the entire thing is air
    if(!slice) {
        b.set();
        return;
    }

    // iterate over every row
    for(size_t z = 0; z < 256; z++) {
        // ignore empty rows
        const size_t zOff = ((z & 0xFF) << 8);
        auto row = slice->rows[z];

        if(!row) {
            for(size_t x = 0; x < 256; x++) {
                b[zOff + x] = true;
            }
            continue;
        }

        // iterate each block in the row to determine if it's air or not
        const auto &airMap = this->exposureIdMaps[row->typeMap];
        for(size_t x = 0; x < 256; x++) {
            const bool isAir = airMap[row->at(x)];
            b[zOff + x] = isAir;
        }
    }
}

/**
 * Generates the mapping of 8-bit block ids to whether they're air or not
 */
void WorldChunk::generateBlockIdMap() {
    PROFILE_SCOPE(GenerateAirMap);

    std::vector<std::array<bool, 256>> maps;
    maps.reserve(this->chunk->sliceIdMaps.size());

    // iterate over each input ID map...
    for(const auto &map : this->chunk->sliceIdMaps) {
        PROFILE_SCOPE(ProcessMap);

        // all blocks should be air by default
        std::array<bool, 256> isAir;
        std::fill(isAir.begin(), isAir.end(), true);

        // then, check each of the UUIDs
        for(size_t i = 0; i < map.idMap.size(); i++) {
            const auto &uuid = map.idMap[i];

            // skip if nil UUID
            if(uuid.is_nil()) {
                continue;
            }

            // TODO: check for solidity
            static std::array<uuids::uuid::value_type, 16> raw = {0x71, 0x4a, 0x92, 0xe3, 
                0x29, 0x84, 0x4f, 0x0e, 0x86, 0x9e, 0x14, 0x16, 0x2d, 0x46, 0x27, 0x60};
            static const uuids::uuid id(raw);

            if(uuid == id) {
                Logging::trace("Id {} is air (uuid {})", i, uuids::to_string(uuid));
                isAir[i] = true;
            } else {
                isAir[i] = false;
            }
        }

        maps.push_back(isAir);
    }

    this->exposureIdMaps = std::move(maps);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// Highlighting stuff
/**
 * Adds a new highlighting section.
 */
uint64_t WorldChunk::addHighlight(const glm::vec3 &start, const glm::vec3 &end) {
    uint64_t id = this->highlightsId++;

    // create highlights info and submit it
    HighlightInfo info;
    info.start = start;
    info.end = end;

    {
        LOCK_GUARD(this->highlightsLock, AddHighlight);
        this->highlights[id] = info;
    }

    // queue highlight buffer updating at the start of next frame
    this->highlightsNeedUpdate = true;
    this->hasHighlights = true;

    return id;
}

/**
 * Removes a highlight with the given id.
 */
bool WorldChunk::removeHighlight(const uint64_t id) {
    LOCK_GUARD(this->highlightsLock, RemoveHighlight);

    bool success = (this->highlights.erase(id) == 1);
    if(success) {
        this->highlightsNeedUpdate = true;
    }

    this->hasHighlights = !this->highlights.empty();
    return success;
}


/**
 * Initializes highlight buffer and vertex arrays
 */
void WorldChunk::initHighlightBuffer() {
    using namespace gfx;

    // allocate them
    this->highlightVao = std::make_shared<VertexArray>();
    this->highlightBuf = std::make_shared<Buffer>(Buffer::Array, Buffer::DynamicDraw);

    this->highlightVao->bind();

    // define per-vertex attributes
    this->vbo->bind();

    this->highlightVao->registerVertexAttribPointer(0, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat), 
            0); // vertex position
    this->highlightVao->registerVertexAttribPointer(1, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat)); // normals
    this->highlightVao->registerVertexAttribPointer(2, 2, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            6 * sizeof(gl::GLfloat)); // texture coordinate

    // clean up
    VertexArray::unbind();
}
/**
 * Update the highlight buffers.
 *
 * Each of the highlights is drawn as a cube stretched to fill the extents.
 */
void WorldChunk::updateHighlightBuffer() {
    PROFILE_SCOPE(UpdateHighlightBuf);
    LOCK_GUARD(this->highlightsLock, RemoveHighlight);

    this->highlightData.clear();

    // iterate over all highlights
    for(const auto &[id, info] : this->highlights) {
        HighlightInstanceData data;

        // calculate the scale/translations
        glm::mat4 translation(1), scaled(1);

        float xScale = fabs(info.start.x - info.end.x);
        float yScale = fabs(info.start.y - info.end.y);
        float zScale = fabs(info.start.z - info.end.z);
        glm::vec3 scale(xScale, yScale, zScale);

        translation = glm::translate(translation, glm::vec3(0.01, 0.01, 0.01));
        translation = glm::translate(translation, info.start);

        translation = glm::translate(translation, glm::vec3(xScale/2, yScale/2, zScale/2));
        scaled = glm::scale(translation, glm::vec3(1.25, 1.25, 1.25));

        // Logging::trace("Scale for extents {},{} -> {}", info.start, info.end, scale);
        translation = glm::scale(translation, scale);
        scaled = glm::scale(scaled, scale);

        data.transform = translation;
        data.scaled = scaled;

        // inscrete it
        this->highlightData.push_back(data);
    }

    // mark buffer as to be updated
    this->highlightsBufDirty = true;
}



/**
 * Draws the highlights. This is done in two steps:
 *
 * 1. Stencil buffer is written to for all selections
 * 2. Rendering each selection slightly scaled up, only where the stencil test passes, with a solid
 *    color allows drawing of the borders.
 */
void WorldChunk::drawHighlights(std::shared_ptr<gfx::RenderProgram> program) {
    using namespace gl;

    PROFILE_SCOPE(DrawHighlights);

    // transfer the highlighting buffer if it's ready, and bail if there's nothing to draw
    if(this->highlightsBufDirty) { // upload buffer
        const auto size = sizeof(HighlightInstanceData) * this->highlightData.size();
        if(size) {
            this->highlightBuf->bind();
            this->highlightBuf->bufferData(size, this->instanceData.data());
            this->highlightBuf->unbind();
        }

        this->numHighlights = this->highlightData.size();
        this->highlightsBufDirty = false;
    }

    if(!this->numHighlights) {
        return;
    }

    // set the highlight color
    program->setUniformVec("HighlightColor", glm::vec3(1, 1, 0));
    program->setUniform1f("WriteColor", 0);

    // step 1: draw to stencil buffer
    // configure to always write a 1 to the appropriate bit in the stencil buffer. no color is written
    glEnable(GL_STENCIL_TEST);
    // glDepthMask(GL_FALSE);
    // glDepthFunc(GL_LEQUAL);
    glDepthFunc(GL_ALWAYS);

    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0x01);
    glStencilMask(0x01);

    this->highlightVao->bind();
    for(const auto &data: this->highlightData) {
        program->setUniformMatrix("model2", data.transform);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // step 2: scale each outline a wee bit and draw the colors where stencil test passes
    program->setUniform1f("WriteColor", 1);

    glStencilFunc(GL_NOTEQUAL, 1, 0x01);
    glStencilMask(0x00); // do not write to stencil buffer
    glDisable(GL_DEPTH_TEST);

    this->highlightVao->bind();
    for(const auto &data: this->highlightData) {
        program->setUniformMatrix("model2", data.scaled);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // clean up
    gfx::VertexArray::unbind();

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

