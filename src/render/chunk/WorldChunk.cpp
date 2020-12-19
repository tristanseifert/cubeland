#include "WorldChunk.h"
#include "WorldChunkDebugger.h"
#include "ChunkWorker.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "world/block/BlockRegistry.h"
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

    // set up the placeholder vertex array
    this->vbo = std::make_shared<Buffer>(Buffer::Array, Buffer::StaticDraw);
    this->vbo->bind();
    this->vbo->bufferData(sizeof(kCubeVertices), (void *) &kCubeVertices);
    this->vbo->unbind();

    this->placeholderVao = std::make_shared<VertexArray>();

    this->vbo->bind();
    this->placeholderVao->registerVertexAttribPointer(0, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat), 
            0); // vertex position
    this->placeholderVao->registerVertexAttribPointer(1, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat)); // normals
    this->placeholderVao->registerVertexAttribPointer(2, 2, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            6 * sizeof(gl::GLfloat)); // texture coordinate

    VertexArray::unbind();

    // set up the block face vertex array
    this->vertexBuf = std::make_shared<Buffer>(Buffer::Array, Buffer::DynamicDraw);
    this->vertexIndexBuf = std::make_shared<Buffer>(Buffer::ElementArray, Buffer::DynamicDraw);

    this->facesVao = std::make_shared<VertexArray>();
    this->facesVao->bind();

    this->vertexBuf->bind();
    this->vertexIndexBuf->bind();

    const size_t kVertexSize = sizeof(BlockVertex);
    this->facesVao->registerVertexAttribPointer(0, 3, VertexArray::Float, kVertexSize,
            0); // vertex position
    this->facesVao->registerVertexAttribPointer(1, 3, VertexArray::Float, kVertexSize,
            3 * sizeof(gl::GLfloat)); // normals
    this->facesVao->registerVertexAttribPointer(2, 2, VertexArray::Float, kVertexSize,
            6 * sizeof(gl::GLfloat)); // texture coordinate

    VertexArray::unbind();

    // set up highlighting and allocate some other buffers
    this->exposureMap.resize((256 * 256 * 256));
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

    if(this->numIndices) {
        if(this->drawWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        this->facesVao->bind();
        this->vertexIndexBuf->bind();

        glDrawElements(GL_TRIANGLES, this->numIndices, GL_UNSIGNED_INT, nullptr);

        gfx::VertexArray::unbind();

        if(this->drawWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    // if no chunk data available, draw a wireframe outline of the chunk
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        this->placeholderVao->bind();
        glDrawArrays(GL_TRIANGLES, 0, 36);
        gfx::VertexArray::unbind();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

/**
 * Transfers dirty buffers to the GPU.
 */
void WorldChunk::transferBuffers() {
    // vertex data for all exposed block faces
    if(this->vertexBufDirty) {
        const auto size = sizeof(BlockVertex) * this->vertexData.size();
        if(size) {
            PROFILE_SCOPE(XferVertexBuf);
            this->vertexBuf->bind();
            this->vertexBuf->bufferData(size, this->vertexData.data());
            this->vertexBuf->unbind();

            Logging::debug("Chunk vertex buf xfer: {} bytes", size);
        } else {
            Logging::warn("Chunk {} vertex buffer is empty", (void *) this->chunk.get());
        }

        this->vertexBufDirty = false;
    }
    // index data for all exposed block faces
    if(this->indexBufDirty) {
        const auto size = sizeof(gl::GLuint) * this->vertexIndexData.size();
        if(size) {
            PROFILE_SCOPE(XferIndexBuf);
            this->vertexIndexBuf->bind();
            this->vertexIndexBuf->bufferData(size, this->vertexIndexData.data());
            this->vertexIndexBuf->unbind();

            this->numIndices = this->vertexIndexData.size();
            Logging::debug("Chunk index buf xfer: {} bytes", size);
        }

        this->indexBufDirty = false;
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

    this->withoutCaching = true;
    this->instanceDataNeedsUpdate = true;
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

        this->vertexData.clear();
        this->vertexIndexData.clear();

        this->exposureIdMaps.clear();
        std::fill(this->exposureMap.begin(), this->exposureMap.end(), false);
    }

    // update the exposure ID maps
    if(this->withoutCaching || this->exposureIdMaps.size() != this->chunk->sliceIdMaps.size()) {
        this->generateBlockIdMap();
        this->exposureMapNeedsUpdate = true;
    }

    // initial air map filling
    AirMap am;
    am.below.reset();
    this->buildAirMap(this->chunk->slices[0], am.current);
    this->buildAirMap(this->chunk->slices[1], am.above);

    // update the actual instance buffer itself
    for(size_t y = 0; y < Chunk::kMaxY; y++) {
        PROFILE_SCOPE(ProcessSlice);
        const size_t yOffset = (y & 0xFF) << 16;

        // if there's no blocks at this Y level, check the next one
        auto slice = this->chunk->slices[y];
        if(!slice) {
            // clear the exposure map
            std::fill(this->exposureMap.begin() + yOffset,
                      this->exposureMap.begin() + (yOffset + 0x10000), false);
            goto nextRow;
        }

        // iterate over each of the slice's rows
        for(size_t z = 0; z < 256; z++) {
            // skip empty rows
            const size_t zOffset = yOffset | ((z & 0xFF) << 8);
            auto row = slice->rows[z];
            if(!row) {
                std::fill(this->exposureMap.begin() + zOffset, 
                          this->exposureMap.begin() + (zOffset + 0x100), false);
                continue;
            }

            // process each block in this row
            const auto &map = this->chunk->sliceIdMaps[row->typeMap];

            for(size_t x = 0; x < 256; x++) {
                // update exposure map for this position if needed
                if(this->exposureMapNeedsUpdate || true) {
                    const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);
                    bool visible = false;

                    // above or below
                    if(am.above[airMapOff]) {
                        visible = true;
                        goto writeResult;
                    }
                    if(am.below[airMapOff]) {
                        visible = true;
                        goto writeResult;
                    }

                    // check adjacent faces
                    for(int i = -1; i <= 1; i+=2) {
                        // left or right
                        if((x == 0 && i == -1) || (x == 255 && i == 1) || am.current[airMapOff + i]) {
                            visible = true;
                            goto writeResult;
                        }
                        // front or back
                        if((z == 0 && i == -1) || (z == 255 && i == 1) || am.current[airMapOff + (i * 256)]) {
                            visible = true;
                            goto writeResult;
                        }
                    }

    writeResult:;
                    // write the result into the exposure map
                    this->exposureMap[zOffset + x] = visible;
                }

                // skip blocks to not draw (e.g. air)
                uint8_t temp = row->at(x);
                const auto &id = map.idMap[temp];
                if(BlockRegistry::isAirBlock(id)) {
                    continue;
                }
                numTotal++;

                // skip block if not exposed
                if(!this->exposureMap[zOffset + x]) {
                    numCulled++;
                    continue;
                }

                // append the vertices for this block
                this->insertBlockVertices(am, x, y, z);
            }
        }

        // set up for processing the next row
nextRow:;
        am.below = std::move(am.current);
        am.current = std::move(am.above);

        if((y+2) < this->chunk->slices.size()) {
            am.above.reset();
            this->buildAirMap(this->chunk->slices[y+2], am.above);
        } else {
            am.above.set();
        }
    }

    // clear dirty flags
    this->exposureMapNeedsUpdate = false;

    // ensure the buffer is transferred on the next frame
    Logging::trace("Wrote {} vertices ({} indices)", this->vertexData.size(),
            this->vertexIndexData.size());

    this->vertexBufDirty = true;
    this->indexBufDirty = true;
}

/**
 * For a visible (e.g. at least one exposed face) block at the given coordinates, insert the
 * necessary vertices to the vertex buffer.
 */
void WorldChunk::insertBlockVertices(const AirMap &am, size_t x, size_t y, size_t z) {
    const size_t yOff = (y & 0xFF) << 16;
    const size_t zOff = yOff | (z & 0xFF) << 8;
    const size_t xOff = zOff | (x & 0xFF);
    const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);

    const glm::vec3 pos(x, y, z);

    gl::GLuint iVtx = this->vertexData.size();

    // is the left edge exposed?
    if(x == 0 || am.current[airMapOff - 1]) {
        const glm::vec3 kNormal(-1, 0, 0);
        this->vertexData.insert(this->vertexData.end(), {
            {pos + glm::vec3(0,0,1), kNormal, glm::vec2(0,1)},
            {pos + glm::vec3(0,1,1), kNormal, glm::vec2(1,1)},
            {pos + glm::vec3(0,1,0), kNormal, glm::vec2(1,0)},
            {pos + glm::vec3(0,0,0), kNormal, glm::vec2(0,0)},
        });
        this->vertexIndexData.insert(this->vertexIndexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the right edge exposed?
    if(x == 255 || am.current[airMapOff + 1]) {
        const glm::vec3 kNormal(1, 0, 0);
        this->vertexData.insert(this->vertexData.end(), {
            {pos + glm::vec3(1,0,0), kNormal, glm::vec2(0,1)},
            {pos + glm::vec3(1,1,0), kNormal, glm::vec2(1,1)},
            {pos + glm::vec3(1,1,1), kNormal, glm::vec2(1,0)},
            {pos + glm::vec3(1,0,1), kNormal, glm::vec2(0,0)},
        });
        this->vertexIndexData.insert(this->vertexIndexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the bottom exposed?
    if(y == 0 || am.below[airMapOff]) {
        const glm::vec3 kNormal(0, -1, 0);
        this->vertexData.insert(this->vertexData.end(), {
            {pos + glm::vec3(0,0,0), kNormal, glm::vec2(0,1)},
            {pos + glm::vec3(1,0,0), kNormal, glm::vec2(1,1)},
            {pos + glm::vec3(1,0,1), kNormal, glm::vec2(1,0)},
            {pos + glm::vec3(0,0,1), kNormal, glm::vec2(0,0)},
        });
        this->vertexIndexData.insert(this->vertexIndexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the top exposed?
    if((y + 1) >= 255 || am.above[airMapOff]) {
        const glm::vec3 kNormal(0, 1, 0);
        this->vertexData.insert(this->vertexData.end(), {
            {pos + glm::vec3(0,1,1), kNormal, glm::vec2(0,1)},
            {pos + glm::vec3(1,1,1), kNormal, glm::vec2(1,1)},
            {pos + glm::vec3(1,1,0), kNormal, glm::vec2(1,0)},
            {pos + glm::vec3(0,1,0), kNormal, glm::vec2(0,0)},
        });
        this->vertexIndexData.insert(this->vertexIndexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the z-1 edge exposed?
    if(z == 0 || am.current[airMapOff - 0x100]) {
        const glm::vec3 kNormal(0, 0, -1);
        this->vertexData.insert(this->vertexData.end(), {
            {pos + glm::vec3(0,1,0), kNormal, glm::vec2(0,1)},
            {pos + glm::vec3(1,1,0), kNormal, glm::vec2(1,1)},
            {pos + glm::vec3(1,0,0), kNormal, glm::vec2(1,0)},
            {pos + glm::vec3(0,0,0), kNormal, glm::vec2(0,0)},
        });
        this->vertexIndexData.insert(this->vertexIndexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the z+2 edge exposed?
    if(z == 255 || am.current[airMapOff + 0x100]) {
        const glm::vec3 kNormal(0,0,1);
        this->vertexData.insert(this->vertexData.end(), {
            {pos + glm::vec3(0,0,1), kNormal, glm::vec2(0,1)},
            {pos + glm::vec3(1,0,1), kNormal, glm::vec2(1,1)},
            {pos + glm::vec3(1,1,1), kNormal, glm::vec2(1,0)},
            {pos + glm::vec3(0,1,1), kNormal, glm::vec2(0,0)},
        });
        this->vertexIndexData.insert(this->vertexIndexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
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

            // query the block registry if this is an air block
            isAir[i] = world::BlockRegistry::isAirBlock(uuid);
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
            this->highlightBuf->bufferData(size, this->highlightData.data());
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

