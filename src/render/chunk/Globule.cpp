#include "Globule.h"
#include "WorldChunk.h"
#include "ChunkWorker.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"
#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"

#include <Logging.h>
#include <mutils/time/profiler.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

using namespace render::chunk;

// uncomment to log buffer transfers
// #define LOG_BUFFER_XFER

/**
 * Initializes a new globule.
 *
 * This allocates the vertex and index buffers, configures a vertex array that can be used for
 * drawing the globule.
 */
Globule::Globule(WorldChunk *_chunk, const glm::ivec3 _pos) : position(_pos), chunk(_chunk) {
    using namespace gfx;

    // allocate buffers
    this->vertexBuf = std::make_shared<Buffer>(Buffer::Array, Buffer::DynamicDraw);
    this->indexBuf = std::make_shared<Buffer>(Buffer::ElementArray, Buffer::DynamicDraw);

    this->facesVao = std::make_shared<VertexArray>();
    this->facesVao->bind();

    this->vertexBuf->bind();
    // this->indexBuf->bind();

    const size_t kVertexSize = sizeof(BlockVertex);
    this->facesVao->registerVertexAttribPointerInt(0, 3, VertexArray::Short, kVertexSize,
            offsetof(BlockVertex, p)); // vertex position
    this->facesVao->registerVertexAttribPointerInt(1, 1, VertexArray::UnsignedShort, kVertexSize,
            offsetof(BlockVertex, blockId)); // block ID
    this->facesVao->registerVertexAttribPointerInt(2, 1, VertexArray::UnsignedByte, kVertexSize,
            offsetof(BlockVertex, face)); // face
    this->facesVao->registerVertexAttribPointerInt(3, 1, VertexArray::UnsignedByte, kVertexSize,
            offsetof(BlockVertex, vertexId)); // vertex id

    VertexArray::unbind();
}

/**
 * Wait for any pending work to complete.
 */
Globule::~Globule() {
    // wait for any background work to complete
    this->abortWork = true;

    for(auto &future : this->futures) {
        future.wait();
    }
    this->futures.clear();
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
        // wait for pending work to complete
        // XXX: this will block the main loop so probably not great but MEH
        for(auto &future : this->futures) {
            future.wait();
        }
        this->futures.clear();

        // only then can we queue the new stuff
        this->abortWork = false;
        this->vertexDataNeedsUpdate = false;
        this->futures.emplace_back(ChunkWorker::pushWork([&]() -> void {
            // clear flag first, so if data changes while we're updating, it's fixed next frame
            this->vertexDataNeedsUpdate = false;
            this->fillBuffer();
        }));
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
    if(!this->chunk || !this->chunk->chunk) return;
    if(this->exposureIdMaps.size() != c->sliceIdMaps.size()) {
        this->generateBlockIdMap();
    }

    // get the chunk pos
    const auto chunkPos = glm::ivec3(this->chunk->chunk->worldPos.x * 256, 0, 
            this->chunk->chunk->worldPos.y * 256);

    // convert the 8 bit -> UUID maps to 8 bit -> block instance maps
    std::vector<std::array<Block *, 256>> blockPtrMaps;
    blockPtrMaps.reserve(c->sliceIdMaps.size());

    {
        PROFILE_SCOPE(BuildBlockPtrMap);
        for(const auto &map : c->sliceIdMaps) {
            std::array<Block *, 256> list;

            for(size_t i = 0; i < list.size(); i++) {
                const auto &id = map.idMap[i];
                if(BlockRegistry::isAirBlock(id)) {
                    continue;
                }

                list[i] = BlockRegistry::getBlock(id);
            }

            blockPtrMaps.push_back(list);
        }
    }

    // initial air map filling
    AirMap am;
    am.below.reset();
    this->buildAirMap(c->slices[this->position.y], am.current);
    this->buildAirMap(c->slices[this->position.y + 1], am.above);

    std::fill(std::begin(this->sliceVertexIdx), std::end(this->sliceVertexIdx), 0);

    // update the actual instance buffer itself
    for(size_t y = this->position.y; y <= std::min((size_t)this->position.y + 64, Chunk::kMaxY-1); y++) {
        PROFILE_SCOPE(ProcessSlice);
        const size_t yOffset = static_cast<size_t>(y - this->position.y) * (64 * 64);

        // bail if needing to exit
        if(!this->chunk || !this->chunk->chunk) return;
        if(this->abortWork) return;

        // record current vertex index
        this->sliceVertexIdx[y - (size_t)this->position.y] = this->vertexData.size();

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
            const auto &blockMap = blockPtrMaps[row->typeMap];

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

                // skip block if not exposed
                if(!visible) {
                    numCulled++;
                    continue;
                }

                // skip blocks to not draw (e.g. air)
                uint8_t temp = row->at(x);
                auto &block = blockMap[temp];
                const auto &id = map.idMap[temp];

                if(!block || BlockRegistry::isAirBlock(id)) {
                    continue;
                }
                numTotal++;

                // figure out what edges are exposed
                Block::BlockFlags flags = Block::kFlagsNone;
                this->flagsForBlock(am, x, y, z, flags);

                // determine block data ID
                const auto worldPos = glm::ivec3(x, y, z) + chunkPos;
                uint16_t type = block->getBlockId(worldPos, flags);

                // append the vertices for this block
                this->insertBlockVertices(am, x, y, z, type);
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

    /*if(!this->indexData.empty() && this->indexData.size() < 65536) {
        Logging::debug("Index data could use 16-bit: {} indices, {} B overhead", 
                this->indexData.size(), this->indexData.size() * 2);
    }*/

    this->vertexBufDirty = true;
    this->indexBufDirty = true;
}

/**
 * Calculates the flags for the given block. Currently, this is just the exposed edges.
 */
void Globule::flagsForBlock(const AirMap &am, const size_t x, const size_t y, const size_t z, world::Block::BlockFlags &flags) {
    using namespace world;

    // calculate offsets into air map
    const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);

    // is the left edge exposed?
    if(x == 0 || am.current[airMapOff - 1]) {
        flags |= Block::kExposedXMinus;
    }
    // is the right edge exposed?
    if(x == 255 || am.current[airMapOff + 1]) {
        flags |= Block::kExposedXPlus;
    }
    // is the bottom exposed?
    if(y == 0 || am.below[airMapOff]) {
        flags |= Block::kExposedYMinus;
    }
    // is the top exposed?
    if((y + 1) >= 255 || am.above[airMapOff]) {
        flags |= Block::kExposedYPlus;
    }
    // is the z-1 edge exposed?
    if(z == 0 || am.current[airMapOff - 0x100]) {
        flags |= Block::kExposedZMinus;
    }
    // is the z+1 edge exposed?
    if(z == 255 || am.current[airMapOff + 0x100]) {
        flags |= Block::kExposedZPlus;
    }
}

/**
 * For a visible (e.g. at least one exposed face) block at the given coordinates, insert the
 * necessary vertices to the vertex buffer.
 *
 * Face IDs for each vertex are assigned as 0xFV, where F is the face (0 = -Y, 1 = +Y, 2+ = sides)
 * and V is the vertex index for that face (0-3). This is used to look up per block information
 * from the block info data texture.
 */
void Globule::insertBlockVertices(const AirMap &am, const size_t x, const size_t y, const size_t z, const uint16_t blockId) {
    const size_t yOff = (y & 0xFF) << 16;
    const size_t zOff = yOff | (z & 0xFF) << 8;
    const size_t xOff = zOff | (x & 0xFF);
    const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);

    const glm::i16vec3 pos(x, y, z);

    gl::GLuint iVtx = this->vertexData.size();

    // is the bottom exposed? (or bottom of globule)
    if(y == 0 || (y % 64) == 0 || am.below[airMapOff]) {
        this->vertexData.insert(this->vertexData.end(), {
            {.p = pos + glm::i16vec3(0,0,0), .blockId = blockId, .face = (0x0), .vertexId = 0},
            {.p = pos + glm::i16vec3(1,0,0), .blockId = blockId, .face = (0x0), .vertexId = 1},
            {.p = pos + glm::i16vec3(1,0,1), .blockId = blockId, .face = (0x0), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,0,1), .blockId = blockId, .face = (0x0), .vertexId = 3},
        });
        this->indexData.insert(this->indexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the top exposed? (or top edge of globule)
    if((y + 1) >= 255 || (y % 64) == 63 || am.above[airMapOff]) {
        this->vertexData.insert(this->vertexData.end(), {
            {.p = pos + glm::i16vec3(0,1,1), .blockId = blockId, .face = (0x1), .vertexId = 0},
            {.p = pos + glm::i16vec3(1,1,1), .blockId = blockId, .face = (0x1), .vertexId = 1},
            {.p = pos + glm::i16vec3(1,1,0), .blockId = blockId, .face = (0x1), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,1,0), .blockId = blockId, .face = (0x1), .vertexId = 3},
        });
        this->indexData.insert(this->indexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the left edge exposed?
    if(x == 0 || am.current[airMapOff - 1]) {
        this->vertexData.insert(this->vertexData.end(), {
            {.p = pos + glm::i16vec3(0,0,1), .blockId = blockId, .face = (0x2), .vertexId = 0},
            {.p = pos + glm::i16vec3(0,1,1), .blockId = blockId, .face = (0x2), .vertexId = 1},
            {.p = pos + glm::i16vec3(0,1,0), .blockId = blockId, .face = (0x2), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,0,0), .blockId = blockId, .face = (0x2), .vertexId = 3},
        });
        this->indexData.insert(this->indexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the right edge exposed?
    if(x == 255 || am.current[airMapOff + 1]) {
        this->vertexData.insert(this->vertexData.end(), {
            {.p = pos + glm::i16vec3(1,0,0), .blockId = blockId, .face = (0x3), .vertexId = 0},
            {.p = pos + glm::i16vec3(1,1,0), .blockId = blockId, .face = (0x3), .vertexId = 1},
            {.p = pos + glm::i16vec3(1,1,1), .blockId = blockId, .face = (0x3), .vertexId = 2},
            {.p = pos + glm::i16vec3(1,0,1), .blockId = blockId, .face = (0x3), .vertexId = 3},
        });
        this->indexData.insert(this->indexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the z-1 edge exposed?
    if(z == 0 || am.current[airMapOff - 0x100]) {
        this->vertexData.insert(this->vertexData.end(), {
            {.p = pos + glm::i16vec3(0,1,0), .blockId = blockId, .face = (0x4), .vertexId = 0},
            {.p = pos + glm::i16vec3(1,1,0), .blockId = blockId, .face = (0x4), .vertexId = 1},
            {.p = pos + glm::i16vec3(1,0,0), .blockId = blockId, .face = (0x4), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,0,0), .blockId = blockId, .face = (0x4), .vertexId = 3},
        });
        this->indexData.insert(this->indexData.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the z+1 edge exposed?
    if(z == 255 || am.current[airMapOff + 0x100]) {
        this->vertexData.insert(this->vertexData.end(), {
            {.p = pos + glm::i16vec3(0,0,1), .blockId = blockId, .face = (0x5), .vertexId = 0},
            {.p = pos + glm::i16vec3(1,0,1), .blockId = blockId, .face = (0x5), .vertexId = 1},
            {.p = pos + glm::i16vec3(1,1,1), .blockId = blockId, .face = (0x5), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,1,1), .blockId = blockId, .face = (0x5), .vertexId = 3},
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
void Globule::buildAirMap(world::ChunkSlice *slice, std::bitset<256*256> &b) {
    // if the slice is empty (e.g. nonexistent,) bail; the entire thing is air
    if(!slice) {
        b.set();
        return;
    }

    // iterate over every row
    for(size_t z = 0; z < 256; z++) {
    // for(size_t z = std::max((int)this->position.z - 1, 0); z < std::min((int)this->position.z + 65, 256); z++) {
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
        for(size_t x = 0; x < 256; x++) {
        // for(size_t x = std::max((int)this->position.x, 0); x < std::min((int)this->position.x + 65, 256); x++) {
            const bool isAir = airMap[row->at(x)];
            b[zOff + x] = isAir;
        }
    }
}

/**
 * Generates the mapping of 8-bit block ids to whether they're air or not
 */
void Globule::generateBlockIdMap() {
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

/**
 * Fills the given texture with normal data for all faces a globule may secrete.
 *
 * For each face of the cube, we generate 4 vertices; this texture is laid out such that the face
 * index indexes into the Y coordinate, while the vertex index (0-3) indexes into the X coordinate;
 * that is to say, the texture is 4x6 in size.
 *
 * In the texture, the RGB component encodes the XYZ of the normal. The alpha component is set to
 * 1, but is not currently used.
 */
void Globule::fillNormalTex(gfx::Texture2D *tex) {
    using namespace gfx;

    std::vector<glm::vec4> data;
    data.resize(4 * 6, glm::vec4(0));

    // static normal data indexed by face
    static const glm::vec3 normals[6] = {
        // bottom
        glm::vec3(0, -1, 0),
        // top
        glm::vec3(0, 1, 0),
        // left
        glm::vec3(-1, 0, 0),
        // right
        glm::vec3(1, 0, 0),
        // Z-1
        glm::vec3(0, 0, -1),
        // Z+1
        glm::vec3(0, 0, 1),
    };

    for(size_t y = 0; y < 6; y++) {
        const size_t yOff = (y * 4);
        for(size_t x = 0; x < 4; x++) {
            data[yOff + x] = glm::vec4(normals[y], 1);
        }
    }

    // allocate texture data and send it
    tex->allocateBlank(4, 6, Texture2D::RGBA16F);
    tex->bufferSubData(4, 6, 0, 0,  Texture2D::RGBA16F, data.data());
}

/**
 * Returns the offset into the vertex buffer for the given block, or -1 if there is no such vertex
 * in the buffer.
 */
int Globule::vertexIndexForBlock(const glm::ivec3 &blockOff) {
    // find the vertex range to check for the Y position

    return -1;
}

