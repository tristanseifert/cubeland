#include "Globule.h"
#include "WorldChunk.h"
#include "ChunkWorker.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/model/RenderProgram.h"
#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "world/block/BlockRegistry.h"

#include <Logging.h>
#include <mutils/time/profiler.h>

using namespace render::chunk;

// uncomment to log buffer transfers
// #define LOG_BUFFER_XFER

/**
 * Initializes a new globule.
 *
 * This allocates the vertex and index buffers, configures a vertex array that can be used for
 * drawing the globule.
 */
Globule::Globule(WorldChunk *_chunk, const glm::vec3 _pos) : position(_pos), chunk(_chunk) {
    using namespace gfx;

    // allocate buffers
    this->vertexBuf = std::make_shared<Buffer>(Buffer::Array, Buffer::DynamicDraw);
    this->indexBuf = std::make_shared<Buffer>(Buffer::ElementArray, Buffer::DynamicDraw);

    this->facesVao = std::make_shared<VertexArray>();
    this->facesVao->bind();

    this->vertexBuf->bind();
    // this->indexBuf->bind();

    const size_t kVertexSize = sizeof(BlockVertex);
    this->facesVao->registerVertexAttribPointer(0, 3, VertexArray::Float, kVertexSize,
            0); // vertex position
    this->facesVao->registerVertexAttribPointer(1, 3, VertexArray::Float, kVertexSize,
            3 * sizeof(gl::GLfloat)); // normals
    this->facesVao->registerVertexAttribPointer(2, 2, VertexArray::Float, kVertexSize,
            6 * sizeof(gl::GLfloat)); // texture coordinate

    VertexArray::unbind();
}

/**
 * Wait for any pending work to complete.
 */
Globule::~Globule() {
    // wait for any background work to complete
    this->abortWork = true;
    // TODO: implement
}

/**
 * Indicates that the chunk we're rendering has changed. All caches should be flushed.
 *
 * Buffers will begin to recompute at the start of the next frame. This is done so that any later
 * calls to this routine in the same frame (for example, from a mass update that didn't properly
 * get coalesced) don't cause us to fill the work queue with superfluous work.
 */
void Globule::chunkChanged(const bool isDifferentChunk) {
    // abort any in-process work
    if(!this->chunk || !this->chunk->chunk || isDifferentChunk) {
        this->abortWork = true;
    }

    // if chunk is different, inhibit drawing til buffers are updated
    if(isDifferentChunk) {
        this->inhibitDrawing = true;
    }

    // clear caches
    this->invalidateCaches = true;
    this->vertexDataNeedsUpdate = true;
}

/**
 * Queues any required background work.
 */
void Globule::startOfFrame() {
    if(this->vertexDataNeedsUpdate) {
        this->abortWork = false;
        this->vertexDataNeedsUpdate = false;
        ChunkWorker::pushWork([&]() -> void {
            // clear flag first, so if data changes while we're updating, it's fixed next frame
            this->vertexDataNeedsUpdate = false;
            this->fillBuffer();
        });
    }
}

/**
 * Draws the globule.
 */
void Globule::draw(std::shared_ptr<gfx::RenderProgram> &program) {
    using namespace gl;

    // transfer any buffers that need it, but only for color render programs
    if(program->rendersColor()) {
        this->transferBuffers();
    }

    // draw if we have indices to do so with
    if(this->isVisible && !this->inhibitDrawing && this->numIndices) {
        this->facesVao->bind();
        this->indexBuf->bind();

        glDrawElements(GL_TRIANGLES, this->numIndices, GL_UNSIGNED_INT, nullptr);

        gfx::VertexArray::unbind();
    }
}

/**
 * Transfers dirty buffers to the GPU.
 */
void Globule::transferBuffers() {
    // vertex data for all exposed block faces
    if(this->vertexBufDirty) {
        const auto size = sizeof(BlockVertex) * this->vertexData.size();
        if(size) {
            PROFILE_SCOPE(XferVertexBuf);
            this->vertexBuf->bind();
            this->vertexBuf->replaceData(size, this->vertexData.data());
            this->vertexBuf->unbind();

#ifdef LOG_BUFFER_XFER
            Logging::debug("Chunk vertex buf xfer: {} bytes", size);
#endif
        }

        this->vertexBufDirty = false;
    }
    // index data for all exposed block faces
    if(this->indexBufDirty) {
        const auto size = sizeof(gl::GLuint) * this->indexData.size();
        if(size) {
            PROFILE_SCOPE(XferIndexBuf);
            this->indexBuf->bind();
            this->indexBuf->replaceData(size, this->indexData.data());
            this->indexBuf->unbind();

#ifdef LOG_BUFFER_XFER
            Logging::debug("Chunk index buf xfer: {} bytes", size);
#endif
        }

        this->numIndices = this->indexData.size();
        this->indexBufDirty = false;
        this->inhibitDrawing = false;
    }
}




/**
 * Fills the globule draw buffer.
 */
void Globule::fillBuffer() {
    using namespace world;
    PROFILE_SCOPE(GlobuleFillBuf);

    if(!this->chunk) return;

    auto c = this->chunk->chunk;
    if(!c) {
        this->vertexData.clear();
        this->indexData.clear();
        this->numIndices = 0;

        return;
    }

    // counters
    size_t numCulled = 0, numTotal = 0;

    // clear caches if needed
    if(this->invalidateCaches) {
        PROFILE_SCOPE(ClearCaches);

        // TODO: should this be double buffered? transfer while we process could cause death
        this->vertexData.clear();
        this->indexData.clear();

        this->exposureIdMaps.clear();
    }

    // update the exposure ID maps
    if(this->exposureIdMaps.size() != c->sliceIdMaps.size()) {
        this->generateBlockIdMap();
    }

    // initial air map filling
    AirMap am;
    am.below.reset();
    this->buildAirMap(c->slices[0], am.current);
    this->buildAirMap(c->slices[1], am.above);

    // update the actual instance buffer itself
    for(size_t y = this->position.y; y < (this->position.y + 64); y++) {
        PROFILE_SCOPE(ProcessSlice);
        const size_t yOffset = static_cast<size_t>(y - this->position.y) * (64 * 64);

        // if there's no blocks at this Y level, check the next one
        auto slice = c->slices[y];
        if(!slice) {
            goto nextRow;
        }

        // iterate over each of the slice's rows
        for(size_t z = this->position.z; z < (this->position.z + 64); z++) {
            // bail if needing to exit
            if(this->abortWork) return;

            // skip empty rows
            const size_t zOffset = yOffset + (static_cast<size_t>(z - this->position.z) * 64);
            auto row = slice->rows[z];
            if(!row) {
                continue;
            }

            // process each block in this row
            const auto &map = c->sliceIdMaps[row->typeMap];

            for(size_t x = this->position.x; x < (this->position.x + 64); x++) {
                bool visible = false;

                // update exposure map for this position if needed
                {
                    const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);

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
                        if((x == this->position.x && i == -1) || (x == (this->position.x+63) && i == 1) || am.current[airMapOff + i]) {
                            visible = true;
                            goto writeResult;
                        }
                        // front or back
                        if((z == this->position.z && i == -1) || (z == (this->position.z+63) && i == 1) || am.current[airMapOff + (i * 256)]) {
                            visible = true;
                            goto writeResult;
                        }
                    }

    writeResult:;
                }

                // skip blocks to not draw (e.g. air)
                uint8_t temp = row->at(x);
                const auto &id = map.idMap[temp];
                if(BlockRegistry::isAirBlock(id)) {
                    continue;
                }
                numTotal++;

                // skip block if not exposed
                if(!visible) {
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

        if((y+2) < c->slices.size()) {
            am.above.reset();
            this->buildAirMap(c->slices[y+2], am.above);
        } else {
            am.above.set();
        }
    }

    // ensure the buffer is transferred on the next frame
    if(!this->vertexData.empty()) {
#ifdef LOG_BUFFER_XFER
        Logging::trace("Wrote {} vertices ({} indices)", this->vertexData.size(),
                this->indexData.size());
#endif
    }

    this->vertexBufDirty = true;
    this->indexBufDirty = true;
}

/**
 * For a visible (e.g. at least one exposed face) block at the given coordinates, insert the
 * necessary vertices to the vertex buffer.
 */
void Globule::insertBlockVertices(const AirMap &am, size_t x, size_t y, size_t z) {
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
        this->indexData.insert(this->indexData.end(), 
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
        this->indexData.insert(this->indexData.end(), 
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
        this->indexData.insert(this->indexData.end(), 
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
        this->indexData.insert(this->indexData.end(), 
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
        this->indexData.insert(this->indexData.end(), 
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
        this->indexData.insert(this->indexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
}

/**
 * For a particular Y layer, generates a bitmap indicating whether the block at the given (Z, X)
 * position is air-like or not.
 *
 * Indices into the bitset are 16-bit 0xZZXX coordinates.
 *
 * We no longer go through all 256 rows/columns, but instead at least 64, and at most 66; one
 * extra on each end, if possible.
 */
void Globule::buildAirMap(std::shared_ptr<world::ChunkSlice> slice, std::bitset<256*256> &b) {
    PROFILE_SCOPE(BuildAirMap);

    // if the slice is empty (e.g. nonexistent,) bail; the entire thing is air
    if(!slice) {
        b.set();
        return;
    }

    // iterate over every row
    for(size_t z = std::max((int)this->position.z - 1, 0); z < std::min((int)this->position.z + 65, 256); z++) {
        // ignore empty rows
        const size_t zOff = ((z & 0xFF) << 8);
        auto row = slice->rows[z];

        if(!row) {
            // fill the entire row anyways; not a lot of overhead here
            for(size_t x = 0; x < 256; x++) {
                b[zOff + x] = true;
            }
            continue;
        }

        // iterate each block in the row to determine if it's air or not
        const auto &airMap = this->exposureIdMaps[row->typeMap];
        for(size_t x = std::max((int)this->position.x, 0); x < std::min((int)this->position.x + 65, 256); x++) {
            const bool isAir = airMap[row->at(x)];
            b[zOff + x] = isAir;
        }
    }
}

/**
 * Generates the mapping of 8-bit block ids to whether they're air or not
 */
void Globule::generateBlockIdMap() {
    PROFILE_SCOPE(GenerateAirMap);

    auto c = this->chunk->chunk;

    std::vector<std::array<bool, 256>> maps;
    maps.reserve(c->sliceIdMaps.size());

    // iterate over each input ID map...
    for(const auto &map : c->sliceIdMaps) {
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

