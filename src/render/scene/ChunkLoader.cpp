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
#include <imgui.h>

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
    this->chunks.clear();

    this->source = source;
}

/**
 * Initializes the displayable chunks.
 */
void ChunkLoader::initDisplayChunks() {
    // get rid of all old chunks 
    this->chunks.clear();
}

/**
 * Perform some general start-of-frame bookkeeping.
 */
void ChunkLoader::startOfFrame() {
    // prune chunk list every 20 or so frames
    if((this->numUpdates % 20) == 0) {
        this->pruneLoadedChunksList();
    }

    // draw the overlay if enabled
    if(this->showsOverlay) {
        this->drawOverlay();
    }
}

/**
 * Called at the start of a frame, this checks to see if we need to load any additional chunks as
 * the player moves.
 */
void ChunkLoader::updateChunks(const glm::vec3 &pos) {
    // perform any deferred chunk loading/unloading
    this->updateDeferredChunks();

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
    for(auto [position, chunk] : this->chunks) {
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

            // Logging::trace("Finished processing for {}: {} (took {:L} µs)", pending.position, (void*) chunk.get(), diffUs);

            // update existing chunk
            if(this->chunks.contains(pending.position)) {
                this->chunks[pending.position]->setChunk(chunk);
            }
            // otherwise, create a new one
            else {
                auto wc = this->makeWorldChunk();
                wc->setChunk(chunk);

                this->chunks[pending.position] = wc;
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
 * Either pops an previous chunk off the chunk queue, or allocates a new one.
 */
std::shared_ptr<render::WorldChunk> ChunkLoader::makeWorldChunk() {
    std::shared_ptr<WorldChunk> chunk = nullptr;

    // get one from the queue if possible
    if(this->chunkQueue.try_dequeue(chunk)) {
        return chunk;
    }

    // fall back to allocation
    return std::make_shared<WorldChunk>();
}

/**
 * Calculates the distance between our current position and all loaded chunks; if it's greater than
 * our internal limit, away they go.
 */
void ChunkLoader::pruneLoadedChunksList() {
    PROFILE_SCOPE(PruneLoadedChunks);

    size_t numChunks = 0, numWorldChunks = 0;

    // remove the stored chunks, if we are able to obtain the lock
    if(this->chunksToDeallocLock.try_lock()){
        PROFILE_SCOPE(PruneChunkData);

        numChunks = std::erase_if(this->loadedChunks, [&](const auto &item) {
            auto const& [pos, chunk] = item;
            const auto distance = std::max(fabs(pos.x - this->centerChunkPos.x), fabs(pos.y - this->centerChunkPos.y));
            bool toRemove = (distance > this->cacheReleaseDistance);
            if(toRemove) {
                this->chunksToDealloc.push_back(chunk);
            }
            return toRemove;
        });

        this->chunksToDeallocLock.unlock();
    }

    // then, the drawing chunks
    {
        PROFILE_SCOPE(PruneWorldChunk);
        numWorldChunks = std::erase_if(this->chunks, [&](const auto &item) {
            auto const& [pos, chunk] = item;
            const auto distance = std::max(fabs(pos.x - this->centerChunkPos.x), fabs(pos.y - this->centerChunkPos.y));
            bool toRemove = (distance > this->chunkRange);
            if(toRemove && this->chunkQueue.size_approx() < this->maxChunkQueueSize) {
                chunk->setChunk(nullptr);
                this->chunkQueue.enqueue(chunk);
            }
            return toRemove;
        });
    }

    if(numChunks || numWorldChunks) {
        Logging::debug("Released {} data chunk(s), {} drawing chunk(s)", numChunks, numWorldChunks);
    }

    // if we removed chunks, queue deallocation
    if(numChunks) {
        auto future = render::chunk::ChunkWorker::pushWork([&] {
            PROFILE_SCOPE(DeallocChunks);
            LOCK_GUARD(this->chunksToDeallocLock, DeallocChunksList);
            this->chunksToDealloc.clear();
        });
    }
}

/**
 * Loads a new chunk for the central area.
 *
 * @return Whether the center chunk changed.
 */
bool ChunkLoader::updateCenterChunk(const glm::vec3 &delta, const glm::vec3 &pos) {
    // ensure we're not duplicating any work by loading the chunk if it already is
    glm::ivec2 camChunk(floor(pos.x / 256), floor(pos.z / 256));

    if(!this->chunks.empty() && this->centerChunkPos == camChunk) {
        return false;
    }

    // request to load the chunk
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
    if(this->chunks.contains(position)) {
        // TODO: should we force chunk to be re-loaded?
        return;
    } else if(std::find(this->currentlyLoading.begin(), this->currentlyLoading.end(), position) != this->currentlyLoading.end()) {
        return;
    }

    // Logging::trace("Requesting loading of chunk {}", position);
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

    for(auto [pos, chunk] : this->chunks) {
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

/**
 * Draws the chunk loader status overlay.
 */
void ChunkLoader::drawOverlay() {
    // distance from the edge of display for the overview
    const float DISTANCE = 10.0f;
    const size_t corner = 1; // top-right
    ImGuiIO& io = ImGui::GetIO();

    // get the window position
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
    ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowSize(ImVec2(230, 0));
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

    ImGui::SetNextWindowBgAlpha(kOverlayAlpha);
    if(!ImGui::Begin("Example: Simple overlay", &this->showsOverlay, window_flags)) {
        return;
    }

    // current camera and chunk positions
    ImGui::Text("Chunk: %d, %d", this->centerChunkPos.x, this->centerChunkPos.y);
    ImGui::SameLine();
    ImGui::Text("Camera: %.1f, %.1f, %.1f", this->lastPos.x, this->lastPos.y, this->lastPos.z);

    // active chunks (data and drawing)
    ImGui::Text("Count: %lu data (%lu pend), %lu draw (%lu cache)", this->loadedChunks.size(),
            this->currentlyLoading.size(), this->chunks.size(), this->chunkQueue.size_approx());

    // chunk work queue items remaining
    ImGui::Text("Work Queue: %lu", chunk::ChunkWorker::getPendingItemCount());

    ImGui::End();
}
