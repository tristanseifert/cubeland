#include "ChunkLoader.h"

#include "render/chunk/WorldChunk.h"
#include "render/chunk/ChunkWorker.h"
#include "world/chunk/Chunk.h"
#include "world/WorldSource.h"
#include "gfx/model/RenderProgram.h"
#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>

#include <algorithm>

using namespace world;
using namespace render::scene;

/**
 * Initializes the chunk loader.
 */
ChunkLoader::ChunkLoader() {
    // set up chunk collection
    this->initDisplayChunks();
}

/**
 * Sets the source from which world data is loaded.
 */
void ChunkLoader::setSource(std::shared_ptr<world::WorldSource> source) {
    // bail if source itself did not change
    if(source == this->source) return;

    // invalidate all chunks
    this->loadedChunks.clear();
    for(auto &chunk : this->chunks) {
        chunk->setChunk(nullptr);
    }

    this->source = source;
}

/**
 * Initializes the displayable chunks.
 */
void ChunkLoader::initDisplayChunks() {
    // get rid of all old chunks 
    this->chunks.clear();

    // set up the central chunk
    this->chunks.push_back(std::make_shared<WorldChunk>());

    // then, the outer "ring" of bonus chunks
    size_t n = (this->chunkRange * 2) + 1;
    size_t numChunks = (n * n) - 1;

    Logging::debug("Chunk range {} -> {}+1 chunks", this->chunkRange, numChunks);

    for(size_t i = 0; i < numChunks; i++) {
        this->chunks.push_back(std::make_shared<WorldChunk>());
    }

    // index of the center chunk
    this->centerIndex = (n * this->chunkRange) + this->chunkRange;
    Logging::debug("Center index: {}", this->centerIndex);
}


/**
 * Called at the start of a frame, this checks to see if we need to load any additional chunks as
 * the player moves.
 */
void ChunkLoader::updateChunks(const glm::vec3 &pos) {
    // perform any deferred chunk loading/unloading
    this->updateDeferredChunks();

    if((this->numUpdates % 15) == 0) {
        // only do this every 15 frames
        this->pruneLoadedChunksList();
    }

    // calculate delta moved. this allows us to skip doing much work if we didn't move
    glm::vec3 delta = pos - this->lastPos;
    this->lastPos = pos;

    if(glm::all(glm::epsilonEqual(delta, glm::vec3(0), kMoveThreshold))) {
        goto noLoad;
    }

    // handle the current central chunk
    if(this->updateCenterChunk(delta, pos)) {
        // recalculate all the surrounding chunks
        for(int xOff = -this->chunkRange; xOff <= (int)this->chunkRange; xOff++) {
            for(int zOff = -this->chunkRange; zOff <= (int)this->chunkRange; zOff++) {
                // if x == z == 0, it's the central chunk and we can skip it
                if(!xOff && !zOff) continue;

                // request the chunk
                auto pos = this->centerChunkPos + glm::ivec2(xOff, zOff);
                this->loadChunk(pos);
            }
        }
    }

noLoad:;
    // update all chunks
    for(auto chunk : this->chunks) {
        chunk->frameBegin();
    }

    this->numUpdates++;
}

/**
 * Updates all chunks whose data became ready since the last invocation.
 *
 * We load chunks on the background on dedicated work queues, and to avoid blocking the render loop
 * while this happens, we don't block on them becoming ready. Instead, each frame, we check if the
 * data has become available; if so, we load it into the appropriate chunk.
 */
void ChunkLoader::updateDeferredChunks() {
    PROFILE_SCOPE(UpdateDeferredChunks);

    // get all finished chunks
    LoadChunkInfo pending;
    while(this->loaded.try_dequeue(pending)) {
        using namespace std::chrono;

        // get how long it took
        const auto now = high_resolution_clock::now();
        const auto diff = now - pending.queuedAt;
        const auto diffUs = duration_cast<microseconds>(diff).count();

        // contains chunk data?
        if(std::holds_alternative<ChunkPtr>(pending.data)) {
            auto chunk = std::get<ChunkPtr>(pending.data);

            Logging::info("Finished processing for {}: {} (took {:L} µs)", pending.position, 
                    (void*) chunk.get(), diffUs);

            // if it's the central chunk, set it there
            if(pending.position == this->centerChunkPos) {
                this->chunks[this->centerIndex]->setChunk(chunk);
            }
            // TODO: figure out which render chunk it go to
            else {
                // convert the offset from the center into an index
                glm::ivec2 diff = pending.position - this->centerChunkPos;
                diff += glm::ivec2(this->chunkRange, this->chunkRange);
                size_t offset = diff.x + (diff.y * ((this->chunkRange * 2) + 1));

                if(offset >= this->chunks.size()) {
                    Logging::info("Chunk pos {} (idx {}) out of bounds", pending.position, offset);
                } else {
                    Logging::trace("Exterior chunk: {} (world pos {}) (index {})", diff,
                            pending.position, offset);

                    // set it
                    this->chunks[offset]->setChunk(chunk);
                }
            }

            // store it in the cache
            this->loadedChunks[pending.position] = chunk;
        }
        // got an error?
        else if(std::holds_alternative<std::exception>(pending.data)) {
            const auto &e = std::get<std::exception>(pending.data);
            Logging::error("Failed to load chunk {}: {}", pending.position, e.what());
        } else {
            XASSERT(false, "Invalid LoadChunkInfo data");
        }

        // regardless, remove it from the "currently loading" list
        this->currentlyLoading.erase(std::remove(this->currentlyLoading.begin(), 
                    this->currentlyLoading.end(), pending.position), this->currentlyLoading.end()); 
    }
}

/**
 * Calculates the distance between our current position and all loaded chunks; if it's greater than
 * our internal limit, away they go.
 */
void ChunkLoader::pruneLoadedChunksList() {
    // TODO: implement this
}

/**
 * Loads a new chunk for the central area.
 *
 * @return Whether the center chunk changed.
 */
bool ChunkLoader::updateCenterChunk(const glm::vec3 &delta, const glm::vec3 &pos) {
    auto &wc = this->chunks[this->centerIndex];

    // ensure we're not duplicating any work by loading the chunk if it already is
    glm::ivec2 camChunk(floor(pos.x / 256), floor(pos.z / 256));

    if(wc->chunk && this->centerChunkPos == camChunk) {
        return false;
    }

    // request to load the chunk
    Logging::trace("Center chunk is {}", camChunk);

    this->centerChunkPos = camChunk;
    this->loadChunk(camChunk);

    return true;
}

/**
 * Requests a background load of the chunk at the given position.
 *
 * This is performed on the background work queue; when completed, a LoadChunkInfo struct is pushed
 * to the main loop to process next frame.
 */
void ChunkLoader::loadChunk(const glm::ivec2 position) {
    // make sure the chunk isn't already loaded or in the process of loading
    if(this->loadedChunks.contains(position)) {
        // if already loaded, push the work item
        LoadChunkInfo info;
        info.position = position;
        info.data = this->loadedChunks[position];
        this->loaded.enqueue(std::move(info));
        return;
    } else if(std::find(this->currentlyLoading.begin(), this->currentlyLoading.end(), position) != this->currentlyLoading.end()) {
        return;
    }

    Logging::trace("Requesting loading of chunk {}", position);
    this->currentlyLoading.push_back(position);

    // push work to the chunk worker queue
    auto future = render::chunk::ChunkWorker::pushWork([&, position] {
        LoadChunkInfo info;
        info.position = position;

        try {
            auto future = this->source->getChunk(position.x, position.y);
            info.data = future.get();
        } catch(std::exception &e) {
            info.data = e;
        }

        // push completion
        this->loaded.enqueue(std::move(info));
    });
}



/**
 * Draws all of the chunks currently loaded.
 */
void ChunkLoader::draw(std::shared_ptr<gfx::RenderProgram> program) {
    const bool withNormals = program->rendersColor();

    for(auto chunk : this->chunks) {
        // ignore chunks without any data
        if(!chunk->chunk) continue;

        // otherwise, draw them
        this->prepareChunk(program, chunk, withNormals);
        chunk->draw(program);
    }
}

/**
 * Prepares a chunk for drawing.
 */
void ChunkLoader::prepareChunk(std::shared_ptr<gfx::RenderProgram> program,
        std::shared_ptr<WorldChunk> chunk, bool hasNormal) {
    auto &c = chunk->chunk;

    // translate based on the chunk's origin
    glm::mat4 model(1);
    model = glm::translate(model, glm::vec3(c->worldPos.x * 256, 0, c->worldPos.y * 256));

    program->setUniformMatrix("model", model);

    // generate the normal matrix
    if(hasNormal) {
        glm::mat3 normalMatrix;
        normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
        program->setUniformMatrix("normalMatrix", normalMatrix);
    }
}

