#include "VertexGenerator.h"
#include "VertexGeneratorData.h"
#include "ChunkWorker.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"

#include "gfx/gl/buffer/Buffer.h"

#include "gui/MainWindow.h"
#include "io/Format.h"
#include "util/Thread.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <SDL.h>
#include <uuid.h>

#include <algorithm>

using namespace render::chunk;

VertexGenerator *VertexGenerator::gShared = nullptr;

/**
 * Sets up the worker thread and background OpenGL queue.
 *
 * This assumes the constructor is called on the main thread, after the main OpenGL context has
 * been created already.
 */
VertexGenerator::VertexGenerator(gui::MainWindow *_window) : window(_window) {
    // create context for the worker
    this->workerGlCtx = SDL_GL_CreateContext(_window->getSDLWindow());
    XASSERT(this->workerGlCtx, "Failed to create vertex generator context: {}", SDL_GetError());

    // start worker
    this->run = true;
    this->worker = std::make_unique<std::thread>(&VertexGenerator::workerMain, this);
}

/**
 * Initializes the shared vertex generator instance.
 */
void VertexGenerator::init(gui::MainWindow *window) {
    XASSERT(!gShared, "Repeated initialization of vertex generator");
    gShared = new VertexGenerator(window);
}

/**
 * Deletes the context we've created.
 *
 * Like the constructor, we assume this is called from the main thread. Deleting contexts from
 * secondary threads is apparently a little fucked.
 */
VertexGenerator::~VertexGenerator() {
    // stop worker thread
    WorkItem quit;

    this->run = false;
    this->submitWorkItem(quit);
    this->worker->join();

    // delete context
    SDL_GL_DeleteContext(this->workerGlCtx);
}

/**
 * Releases the shared vertex generator instance.
 */
void VertexGenerator::shutdown() {
    XASSERT(gShared, "Repeated shutdown of vertex generator");
    delete gShared;
    gShared = nullptr;
}



/**
 * Registers a new callback function.
 */
uint32_t VertexGenerator::addCallback(const glm::ivec2 &chunkPos, const Callback &func) {
    PROFILE_SCOPE(AddVtxGenCb);

    // build callback info struct
    CallbackInfo cb;
    cb.chunk = chunkPos;
    cb.callback = func;

    // generate ID and insert it
    uint32_t id = this->nextCallbackId++;
    {
        LOCK_GUARD(this->callbacksLock, Callbacks);
        this->callbacks[id] = cb;
    }

    // update the chunk callback mapping
    {
        LOCK_GUARD(this->chunkCallbackMapLock, ChunkCbMap);
        this->chunkCallbackMap.emplace(chunkPos, id);
    }

    // Logging::trace("Adding vtx gen callback for chunk {} (id ${:x})", chunkPos, id);
    return id;
}

/**
 * Removes a previously registered callback function.
 */
void VertexGenerator::removeCallback(const uint32_t token) {
    PROFILE_SCOPE(RemoveVtxGenCb);

    // Logging::trace("Removing vtx gen callback with token ${:x}", token);

    // erase it from the chunk callback mapping
    {
        LOCK_GUARD(this->chunkCallbackMapLock, ChunkCbMap);
        const auto count = std::erase_if(this->chunkCallbackMap, [token](const auto &item) {
            return (item.second == token);
        });
        XASSERT(count, "No callback with token ${:x} in chunk->callback map", token);
    }

    // then, actually remove the callback
    {
        LOCK_GUARD(this->callbacksLock, Callbacks);
        const auto count = this->callbacks.erase(token);
        XASSERT(count, "No callback with token ${:x} registered", token);
    }
}

/**
 * Kicks off vertex generation for the given chunk, generating data for all globules in the
 * bitmask.
 */
void VertexGenerator::generate(std::shared_ptr<world::Chunk> &chunk, const uint64_t bits, const bool highPriority) {
    // queue a generating request
    GenerateRequest req;
    req.chunk = chunk;
    req.globules = bits;

    // submit the work item
    WorkItem i;
    i.payload = req;

    if(!highPriority) {
        this->submitWorkItem(i);
    } else {
        this->highPriorityWork.enqueue(i);

        // to wake up the thread if it's asleeping...
        WorkItem dummy;
        this->workQueue.enqueue(dummy);
    }
}



/**
 * Main loop of the worker thread
 */
void VertexGenerator::workerMain() {
    // make context current
    SDL_GL_MakeCurrent(this->window->getSDLWindow(), this->workerGlCtx);

    util::Thread::setName("VtxGen Worker");
    MUtils::Profiler::NameThread("Vertex Generator");

    // as long as desired, perform work items
    while(this->run) {
        // block on dequeuing a work item
        WorkItem item;
        bool highPriority = false;
        {
            PROFILE_SCOPE_STR("WaitWork", 0xFF000050);
            if(this->highPriorityWork.try_dequeue(item)) {
                highPriority = true;
                goto process;
            }
            this->workQueue.wait_dequeue(item);
        }

process:;
        const auto &p = item.payload;

        // no-op
        if(std::holds_alternative<std::monostate>(p)) {
            // nothing. duh
        }
        // generate data for the given globules
        else if(std::holds_alternative<GenerateRequest>(p)) {
            const auto &gr = std::get<GenerateRequest>(p);
            this->workerGenerate(gr, !highPriority);
        }
        // unknown
        else {
            XASSERT(false, "Unknown vertex generator work item payload");
        }
    }

    // detach context
    SDL_GL_MakeCurrent(this->window->getSDLWindow(), nullptr);
}

/**
 * Performs generation of the given chunk's data.
 */
void VertexGenerator::workerGenerate(const GenerateRequest &req, const bool useChunkWorker) {
    // for each globule, queue generation in the background if needed
    for(size_t y = 0; y < 256; y += 64) {
        for(size_t z = 0; z < 256; z += 64) {
            for(size_t x = 0; x < 256; x += 64) {
                // ensure that this bit is set
                const glm::ivec3 origin(x, y, z);
                const uint64_t bits = blockPosToBits(origin);
                if((bits & req.globules) == 0) continue;

                // bail if already processing it
                if(!useChunkWorker) {
                    LOCK_GUARD(this->inFlightLock, InFlight);
                    std::pair<glm::ivec2, glm::ivec3> test(req.chunk->worldPos, origin);

                    if(this->inFlight.contains(test)) {
                        return;
                    }

                    this->inFlight.insert(std::move(test));
                }

                // handle generation
                auto chunk = req.chunk;
                auto fxn = [&, chunk, origin](const bool uiUpdate = false) -> void {
                    try {
                        this->workerGenerate(chunk, origin, uiUpdate);
                    } catch(const std::exception &e) {
                        Logging::error("Error generating globule: {}", e.what());
                        throw;
                    }
                };

                if(useChunkWorker) {
                    ChunkWorker::pushWork(fxn);
                } else {
                    this->highPriorityWorkQueue.queueWorkItem(std::bind(fxn, true));
                }
            }
        }
    }
}

/**
 * Generates vertices for the given globule using the CPU on the chunk worker queue.
 */
void VertexGenerator::workerGenerate(const std::shared_ptr<world::Chunk> &chunk, const glm::ivec3 &origin, const bool highPriority) {
    using namespace world;

    PROFILE_SCOPE(GenerateGlobule);
    // Logging::trace("Generating {} for {}", origin, (void *) chunk.get());

    // counters
    size_t numCulled = 0, numTotal = 0;
    // get the chunk pos
    const auto chunkPos = glm::ivec3(chunk->worldPos.x * 256, 0, chunk->worldPos.y * 256);

    // convert the 8 bit block ID -> UUID maps into 8 bit ID -> block transparency
    ExposureMaps exposureIdMaps;
    this->generateBlockIdMap(chunk, exposureIdMaps);

    // convert the 8 bit -> UUID maps to 8 bit -> block instance maps
    std::vector<std::array<Block *, 256>> blockPtrMaps;
    blockPtrMaps.reserve(chunk->sliceIdMaps.size());

    {
        PROFILE_SCOPE(BuildBlockPtrMap);
        for(const auto &map : chunk->sliceIdMaps) {
            std::array<Block *, 256> list;
            std::fill(list.begin(), list.end(), nullptr);

            for(size_t i = 0; i < list.size(); i++) {
                const auto &id = map.idMap[i];
                if(id.is_nil() || BlockRegistry::isAirBlock(id)) {
                    continue;
                }

                list[i] = BlockRegistry::getBlock(id);
            }

            blockPtrMaps.push_back(list);
        }
    }

    // temporary index data buffer. we'll either take this as-is or convert to 16-bit later
    std::vector<gl::GLuint> indices, indicesSpecial;
    std::vector<BlockVertex> vertices;

    // initial air map filling
    AirMap am;
    am.below.reset();
    this->buildAirMap(chunk->slices[origin.y], exposureIdMaps, am.current);
    this->buildAirMap(chunk->slices[origin.y + 1], exposureIdMaps, am.above);

    // update the actual instance buffer itself
    {
        PROFILE_SCOPE(ProcessSlices);
        const size_t yMax = std::min((size_t) origin.y + 64, Chunk::kMaxY-1);
        for(size_t y = origin.y; y <= yMax; y++) {
            // if there's no blocks at this Y level, check the next one
            auto slice = chunk->slices[y];
            if(!slice) {
                goto nextRow;
            }

            // iterate over each of the slice's rows
            for(size_t z = origin.z; z < (origin.z + 64); z++) {
                // skip empty rows
                auto row = slice->rows[z];
                if(!row) {
                    continue;
                }

                // process each block in this row
                const auto &map = chunk->sliceIdMaps[row->typeMap];
                const auto &blockMap = blockPtrMaps[row->typeMap];

                for(size_t x = origin.x; x < (origin.x + 64); x++) {
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
                            if((x == origin.x && i == -1) || (x == (origin.x+63) && i == 1) || am.current[airMapOff + i]) {
                                visible = true;
                                goto writeResult;
                            }
                            // front or back
                            if((z == origin.z && i == -1) || (z == (origin.z+63) && i == 1) || am.current[airMapOff + (i * 256)]) {
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
                    const uint16_t type = block->getBlockId(worldPos, flags);

                    // append the vertices for this block
                    const uint16_t model = block->getModelId(worldPos, flags);

                    const bool special = block->needsAlphaBlending(worldPos);

                    if(model == 0) {
                        this->insertCubeVertices(am, vertices, special ? indicesSpecial : indices,
                                x, y, z, type);
                    } else if(BlockRegistry::hasModel(model)) {
                        const auto &modelData = BlockRegistry::getModel(model);
                        this->insertModelVertices(am, vertices, special ? indicesSpecial : indices,
                                x, y, z, type, modelData);

                        block->blockWillDisplay(worldPos);
                    } else {
                        XASSERT(false, "Unknown model id ${:04x} for block {}", model, worldPos);
                    }
                }
            }

            // set up for processing the next row
    nextRow:;
            am.below = std::move(am.current);
            am.current = std::move(am.above);

            if((y+2) < chunk->slices.size() && y != yMax) {
                am.above.reset();
                this->buildAirMap(chunk->slices[y+2], exposureIdMaps, am.above);
            } else {
                am.above.set();
            }
        }
    }

    // insert the special indices if needed
    size_t specialStart = 0;

    if(indicesSpecial.size()) {
        specialStart = indices.size();
        indices.insert(indices.end(), indicesSpecial.begin(), indicesSpecial.end());
    }

    // convert indices to 16-bit quantity, if required
    BufferRequest req;
    req.chunkPos = chunk->worldPos;
    req.globuleOff = origin;
    req.specialIdxOffset = specialStart;

    if(!indices.empty() && indices.size() < 65536) {
        std::vector<gl::GLushort> shortIndices;
        shortIndices.resize(indices.size());

        for(size_t i = 0; i < indices.size(); i++) {
            shortIndices[i] = indices[i];
        }

        req.indices = std::move(shortIndices);
    } else {
        req.indices = std::move(indices);
    }

    req.vertices = std::move(vertices);

    this->bufferReqs.enqueue(req);
}



/**
 * Generates the mapping of 8-bit block ids to whether they're air or not
 */
void VertexGenerator::generateBlockIdMap(const std::shared_ptr<world::Chunk> &c, ExposureMaps &maps) {
    maps.clear();
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

            // query the block registry if this is an opaque block
            isAir[i] = !world::BlockRegistry::isOpaqueBlock(uuid);
        }

        maps.push_back(isAir);
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
void VertexGenerator::buildAirMap(world::ChunkSlice *slice, const ExposureMaps &exposureMaps, std::bitset<256*256> &b) {
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
        const auto &airMap = exposureMaps[row->typeMap];
        for(size_t x = 0; x < 256; x++) {
        // for(size_t x = std::max((int)this->position.x, 0); x < std::min((int)this->position.x + 65, 256); x++) {
            const bool isAir = airMap[row->at(x)];
            b[zOff + x] = isAir;
        }
    }
}

/**
 * Calculates the flags for the given block. Currently, this is just the exposed edges.
 */
void VertexGenerator::flagsForBlock(const AirMap &am, const size_t x, const size_t y, const size_t z, world::Block::BlockFlags &flags) {
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
 * Note that this works only for FULLY SOLID blocks, e.g. ones where they want to look like a
 * textured cube.
 */
void VertexGenerator::insertCubeVertices(const AirMap &am, std::vector<BlockVertex> &vertices, std::vector<gl::GLuint> &indices, const size_t x, const size_t y, const size_t z, const uint16_t blockId) {
    const size_t airMapOff = ((z & 0xFF) << 8) | (x & 0xFF);

    const uint16_t f = BlockVertex::kPointFactor;
    const glm::i16vec3 pos(x * f, y * f, z * f);

    gl::GLuint iVtx = vertices.size();

    // is the bottom exposed? (or bottom of globule)
    if(y == 0 || (y % 64) == 0 || am.below[airMapOff]) {
        vertices.insert(vertices.end(), {
            {.p = pos + glm::i16vec3(0,0,0), .blockId = blockId, .face = (0x0), .vertexId = 0},
            {.p = pos + glm::i16vec3(f,0,0), .blockId = blockId, .face = (0x0), .vertexId = 1},
            {.p = pos + glm::i16vec3(f,0,f), .blockId = blockId, .face = (0x0), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,0,f), .blockId = blockId, .face = (0x0), .vertexId = 3},
        });
        indices.insert(indices.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the top exposed? (or top edge of globule)
    if((y + 1) >= 255 || (y % 64) == 63 || am.above[airMapOff]) {
        vertices.insert(vertices.end(), {
            {.p = pos + glm::i16vec3(0,f,f), .blockId = blockId, .face = (0x1), .vertexId = 0},
            {.p = pos + glm::i16vec3(f,f,f), .blockId = blockId, .face = (0x1), .vertexId = 1},
            {.p = pos + glm::i16vec3(f,f,0), .blockId = blockId, .face = (0x1), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,f,0), .blockId = blockId, .face = (0x1), .vertexId = 3},
        });
        indices.insert(indices.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the left edge exposed?
    if(x == 0 || am.current[airMapOff - 1]) {
        vertices.insert(vertices.end(), {
            {.p = pos + glm::i16vec3(0,0,f), .blockId = blockId, .face = (0x2), .vertexId = 0},
            {.p = pos + glm::i16vec3(0,f,f), .blockId = blockId, .face = (0x2), .vertexId = 1},
            {.p = pos + glm::i16vec3(0,f,0), .blockId = blockId, .face = (0x2), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,0,0), .blockId = blockId, .face = (0x2), .vertexId = 3},
        });
        indices.insert(indices.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the right edge exposed?
    if(x == 255 || am.current[airMapOff + 1]) {
        vertices.insert(vertices.end(), {
            {.p = pos + glm::i16vec3(f,0,0), .blockId = blockId, .face = (0x3), .vertexId = 0},
            {.p = pos + glm::i16vec3(f,f,0), .blockId = blockId, .face = (0x3), .vertexId = 1},
            {.p = pos + glm::i16vec3(f,f,f), .blockId = blockId, .face = (0x3), .vertexId = 2},
            {.p = pos + glm::i16vec3(f,0,f), .blockId = blockId, .face = (0x3), .vertexId = 3},
        });
        indices.insert(indices.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the z-1 edge exposed?
    if(z == 0 || am.current[airMapOff - 0x100]) {
        vertices.insert(vertices.end(), {
            {.p = pos + glm::i16vec3(0,f,0), .blockId = blockId, .face = (0x4), .vertexId = 0},
            {.p = pos + glm::i16vec3(f,f,0), .blockId = blockId, .face = (0x4), .vertexId = 1},
            {.p = pos + glm::i16vec3(f,0,0), .blockId = blockId, .face = (0x4), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,0,0), .blockId = blockId, .face = (0x4), .vertexId = 3},
        });
        indices.insert(indices.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
    // is the z+1 edge exposed?
    if(z == 255 || am.current[airMapOff + 0x100]) {
        vertices.insert(vertices.end(), {
            {.p = pos + glm::i16vec3(0,0,f), .blockId = blockId, .face = (0x5), .vertexId = 0},
            {.p = pos + glm::i16vec3(f,0,f), .blockId = blockId, .face = (0x5), .vertexId = 1},
            {.p = pos + glm::i16vec3(f,f,f), .blockId = blockId, .face = (0x5), .vertexId = 2},
            {.p = pos + glm::i16vec3(0,f,f), .blockId = blockId, .face = (0x5), .vertexId = 3},
        });
        indices.insert(indices.end(), 
                {iVtx, iVtx+1, iVtx+2, iVtx+2, iVtx+3, iVtx});
        iVtx += 4;
    }
}

void VertexGenerator::insertModelVertices(const AirMap &am, std::vector<BlockVertex> &vertices, std::vector<gl::GLuint> &indices, const size_t x, const size_t y, const size_t z, const uint16_t blockId, const world::BlockRegistry::Model &model) {
    const uint16_t f = BlockVertex::kPointFactor;
    const glm::i16vec3 origin(x * f, y * f, z * f);

    gl::GLuint iVtx = vertices.size();

    // create vertices
    for(size_t i = 0; i < model.vertices.size(); i++) {
        const auto &vtx = model.vertices[i];
        const auto &faceInfo = model.faceVertIds[i];

        const auto pos = origin + glm::i16vec3(vtx * glm::vec3(f));

        const BlockVertex vtxData = {
            .p = pos, .blockId = blockId, .face = faceInfo.first, .vertexId = faceInfo.second
        };
        vertices.push_back(vtxData);
    }

    // copy the indices
    for(size_t i = 0; i < model.indices.size(); i++) {
        indices.emplace_back(model.indices[i] + iVtx);
    }
}



/**
 * Runs a certain number of globule buffer filling operations on the main thread.
 */
void VertexGenerator::copyBuffers() {
    PROFILE_SCOPE(CopyChunkBufs);

    for(size_t i = 0; i < this->maxCopiesPerFrame; i++) {
        BufferRequest req;
        if(this->bufferReqs.try_dequeue(req)) {
            this->workerGenBuffers(req);
        }
    }
}

/**
 * Builds OpenGL buffers for the given vertex and index buffers. The appropriate callback
 * methods are invoked as well.
 */
void VertexGenerator::workerGenBuffers(const BufferRequest &req) {
    Buffer outBuf;
    outBuf.numVertices = req.vertices.size();
    outBuf.specialIdxOffset = req.specialIdxOffset;

    // create vertex buffer
    const auto vtxSize = sizeof(BlockVertex) * req.vertices.size();
    if(vtxSize) {
        PROFILE_SCOPE(XferVertexBuf);

        auto buf = std::make_shared<gfx::Buffer>(gfx::Buffer::Array, gfx::Buffer::StaticDraw);

        buf->bind();
        buf->replaceData(vtxSize, req.vertices.data());
        buf->unbind();

        outBuf.buffer = buf;
    }

    // then, the index data buffer
    {
        void const *ptr = nullptr;
        size_t size = 0;

        if(std::holds_alternative<std::vector<gl::GLuint>>(req.indices)) {
            const auto &vec = std::get<std::vector<gl::GLuint>>(req.indices);
            outBuf.numIndices = vec.size();

            outBuf.bytesPerIndex = sizeof(gl::GLuint);
            size = sizeof(gl::GLuint) * vec.size();
            ptr = vec.data();
        } else if(std::holds_alternative<std::vector<gl::GLushort>>(req.indices)) {
            const auto &vec = std::get<std::vector<gl::GLushort>>(req.indices);
            outBuf.numIndices = vec.size();

            outBuf.bytesPerIndex = sizeof(gl::GLushort);
            size = sizeof(gl::GLushort) * vec.size();
            ptr = vec.data();
        }

        if(ptr && size) {
            PROFILE_SCOPE(XferIndexBuf);

            auto buf = std::make_shared<gfx::Buffer>(gfx::Buffer::ElementArray, gfx::Buffer::StaticDraw);

            buf->bind();
            buf->replaceData(size, ptr);
            buf->unbind();

            outBuf.indexBuffer = buf;
        }
    }

    // remove the in-flight tag if any
    {
        LOCK_GUARD(this->inFlightLock, InFlight);
        std::pair<glm::ivec2, glm::ivec3> test(req.chunkPos, req.globuleOff);
        this->inFlight.erase(test);
    }

    // invoke the appropriate callbacks
    std::vector<CallbackInfo> cbs;

    {
        LOCK_GUARD(this->chunkCallbackMapLock, ChunkCbMap);
        LOCK_GUARD(this->callbacksLock, Callbacks);
        auto range = this->chunkCallbackMap.equal_range(req.chunkPos);

        for(auto it = range.first; it != range.second; it++) {
            const auto token = it->second;
            cbs.push_back(this->callbacks.at(token));
        }
    }

    PROFILE_SCOPE(InvokeCallbacks);

    if(!cbs.empty()) {
        BufList bufs;
        bufs.emplace_back(req.globuleOff, std::move(outBuf));

        for(auto &cb : cbs) {
            cb.callback(req.chunkPos, bufs);
        }
    }
}

