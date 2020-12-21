#include "ChunkLoader.h"

#include "render/chunk/WorldChunk.h"
#include "render/chunk/Globule.h"
#include "render/chunk/ChunkWorker.h"
#include "world/chunk/Chunk.h"
#include "world/WorldSource.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"
#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <imgui.h>

#include <algorithm>

using namespace world;
using namespace render::scene;

/**
 * Initializes the chunk loader.
 */
ChunkLoader::ChunkLoader() {
    // allocate the face ID -> normal coordinate texture
    this->globuleNormalTex = std::make_shared<gfx::Texture2D>(1);
    this->globuleNormalTex->setUsesLinearFiltering(false);
    this->globuleNormalTex->setDebugName("ChunkNormalMap");

    chunk::Globule::fillNormalTex(this->globuleNormalTex);

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
    this->visibilityMap.clear();

    this->source = source;
}

/**
 * Initializes the displayable chunks.
 */
void ChunkLoader::initDisplayChunks() {
    // get rid of all old chunks 
    this->chunks.clear();
    this->visibilityMap.clear();
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
    if(this->showsChunkList) {
        this->drawChunkList();
    }
}

/**
 * Called at the start of a frame, this checks to see if we need to load any additional chunks as
 * the player moves.
 */
void ChunkLoader::updateChunks(const glm::vec3 &pos, const glm::vec3 &viewDirection) {
    bool visibilityChanged = false;

    // perform any deferred chunk loading/unloading
    this->updateDeferredChunks();

    // update which chunks are visible at the moment only if the direction changed enough
    glm::vec3 dirDelta = viewDirection - this->lastDirection;
    if(!glm::all(glm::epsilonEqual(dirDelta, glm::vec3(0), kDirectionThreshold))) {
        this->lastDirection = viewDirection;

        visibilityChanged = this->updateVisible(pos);
    }

    // if position update was significant, update both it AND the visibility map
    glm::vec3 delta = pos - this->lastPos;
    if(!glm::all(glm::epsilonEqual(delta, glm::vec3(0), kMoveThreshold))) {
        this->lastPos = pos;

        if(!visibilityChanged) {
            visibilityChanged = this->updateVisible(pos);
        }
    }

    // handle the current central chunk
    if(this->updateCenterChunk(delta, pos) || visibilityChanged) {
        // recalculate all the surrounding chunks
        for(int xOff = -this->chunkRange; xOff <= (int)this->chunkRange; xOff++) {
            for(int zOff = -this->chunkRange; zOff <= (int)this->chunkRange; zOff++) {
                const auto chunkPos = this->centerChunkPos + glm::ivec2(xOff, zOff);

                // if x == z == 0, it's the central chunk and we can skip it
                if(!xOff && !zOff) continue;
                // skip if not visible
                if(!this->visibilityMap[chunkPos]) continue;

                // request the chunk
                this->loadChunk(chunkPos);
            }
        }
    }

noLoad:;
    // update all chunks
    for(auto [position, info] : this->chunks) {
        info.wc->frameBegin();
    }

    this->numUpdates++;
}

/**
 * Updates the visibility map from the current camera perspective.
 *
 * Note that this not only straight ahead direction for the camera is tried, but also ones offset
 * by ±(1/2)FoV. This will certainly catch some extra chunks, but definitely make sure all chunks
 * at the edge of the view are marked as visible.
 */
bool ChunkLoader::updateVisible(const glm::vec3 &cameraPos) {
    // precalculate the direction vectors
    std::vector<glm::vec3> dirfracs;

    dirfracs.emplace_back(1.f/(this->lastDirection.x+FLT_MIN), 1.f/(this->lastDirection.y+FLT_MIN),
            1.f/(this->lastDirection.z+FLT_MIN));

    if(this->fov > 0) {
        glm::vec3 left = glm::rotate(this->lastDirection, -glm::radians(this->fov/1.66f), glm::vec3(1,0,1));
        dirfracs.emplace_back(1.f/(left.x+FLT_MIN), 1.f/(left.y+FLT_MIN), 1.f/(left.z+FLT_MIN));

        glm::vec3 right = glm::rotate(this->lastDirection, glm::radians(this->fov/1.66f), glm::vec3(1,0,1));
        dirfracs.emplace_back(1.f/(right.x+FLT_MIN), 1.f/(right.y+FLT_MIN), 1.f/(right.z+FLT_MIN));
    }

    // check each in-bounds chunk
    const glm::vec2 centerChunk = glm::vec2(cameraPos.x/256., cameraPos.z/256.) * glm::vec2(256., 256.);

    for(int xOff = -this->chunkRange; xOff <= (int)this->chunkRange; xOff++) {
        for(int zOff = -this->chunkRange; zOff <= (int)this->chunkRange; zOff++) {
            // calculate its bounding rect
            const glm::vec2 chunkPos = centerChunk + glm::vec2(xOff * 256., zOff * 256.);
            const glm::vec3 lb(chunkPos.x, 0, chunkPos.y), rt(chunkPos.x+256., 256, chunkPos.y+256.);

            // check for intersection
            bool intersects = false;
            for(const auto &dirfrac : dirfracs) {
                intersects = this->checkIntersect(cameraPos, dirfrac, lb, rt);
                if(intersects) goto beach;
            }

beach:;
            const auto key = this->centerChunkPos + glm::ivec2(xOff, zOff);
            this->visibilityMap.insert_or_assign(key, intersects);
            // Logging::trace("Check visibility of chunk {} (off ({}, {})) bounds {} {} -> {}", chunkPos, xOff, zOff, lb, rt, intersects);
        }
    }

    // TODO: determine if anything actually did change
    return true;
}

/**
 * Checks whether the given direction (in this case, an inverse direction vector) intersects the
 * volume defined by the lower/bottom (lb) and right/top (rt) coordinates.
 */
bool ChunkLoader::checkIntersect(const glm::vec3 &org, const glm::vec3 &dirfrac, const glm::vec3 &lb, const glm::vec3 &rt) {
    float t1 = (lb.x - org.x)*dirfrac.x;
    float t2 = (rt.x - org.x)*dirfrac.x;
    float t3 = (lb.y - org.y)*dirfrac.y;
    float t4 = (rt.y - org.y)*dirfrac.y;
    float t5 = (lb.z - org.z)*dirfrac.z;
    float t6 = (rt.z - org.z)*dirfrac.z;

    float tmin = fmax(fmax(fmin(t1, t2), fmin(t3, t4)), fmin(t5, t6));
    float tmax = fmin(fmin(fmax(t1, t2), fmax(t3, t4)), fmax(t5, t6));
    float t = 0;

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if (tmax < 0) {
        t = tmax;
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax) {
        t = tmax;
        return false;
    }

    t = tmin;
    return true;
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

            // skip assigning it to a draw chunk if it is not currently visible
            if(this->visibilityMap.contains(pending.position) && this->visibilityMap[pending.position]) {
                // update existing chunk
                if(this->chunks.contains(pending.position)) {
                    // this->chunks[pending.position].wc->setChunk(chunk);
                }
                // otherwise, create a new one
                else {
                    auto wc = this->makeWorldChunk();
                    wc->setChunk(chunk);

                    RenderChunk info;
                    info.wc = wc;

                    this->chunks[pending.position] = info;
                }
            }

            // regardless, store it in the cache
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
    PROFILE_SCOPE(PruneChunks);

    size_t numChunks = 0, numWorldChunks = 0, numVisibility = 0;

    // remove the stored chunks, if we are able to obtain the lock
    if(this->chunksToDeallocLock.try_lock()){
        PROFILE_SCOPE(DataChunk);

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
        PROFILE_SCOPE(DrawChunk);
        numWorldChunks = std::erase_if(this->chunks, [&](const auto &item) {
            auto const& [pos, info] = item;
            const auto distance = std::max(fabs(pos.x - this->centerChunkPos.x), fabs(pos.y - this->centerChunkPos.y));
            bool toRemove = (distance > this->chunkRange);
            if(toRemove && this->chunkQueue.size_approx() < this->maxChunkQueueSize) {
                info.wc->setChunk(nullptr);
                this->chunkQueue.enqueue(info.wc);
            }
            return toRemove;
        });
    }

    // garbage collect the visibility map
    {
        PROFILE_SCOPE(VisibilityMap);
        numWorldChunks = std::erase_if(this->visibilityMap, [&](const auto &item) {
            auto const& [pos, visible] = item;
            const auto distance = std::max(fabs(pos.x - this->centerChunkPos.x), fabs(pos.y - this->centerChunkPos.y));
            return (distance > this->visibilityReleaseDistance);
        });

    }

    if(numChunks || numWorldChunks) {
        Logging::debug("Released {} data chunk(s), {} drawing chunk(s), {} visibility map entries", 
                numChunks, numWorldChunks, numVisibility);
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
    // ignore requests to load on-screen chunks
    if(this->chunks.contains(position)) {
        return;
    }
    // if the chunk is not visible, but we've loaded it, pretend it has just finished loading
    else if(this->loadedChunks.contains(position)) {
        LoadChunkInfo info;
        info.position = position;
        info.data = this->loadedChunks[position];
        this->loaded.enqueue(std::move(info));

        return;
    }
    // if the chunk is currently being loaded, exit
    else if(std::find(this->currentlyLoading.begin(), this->currentlyLoading.end(), position) != this->currentlyLoading.end()) {
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
void ChunkLoader::draw(std::shared_ptr<gfx::RenderProgram> program, const glm::vec3 &viewDirection) {
    const bool withNormals = program->rendersColor();

    // bind data textures/buffers
    if(program->rendersColor()) {
        this->globuleNormalTex->bind();
        program->setUniform1i("vtxNormalTex", this->globuleNormalTex->unit);
    }

    // draw chunks
    for(auto [pos, info] : this->chunks) {
        // ignore chunks without any data or if they're invisible
        if(!info.wc || !info.wc->chunk) continue;
        else if(!info.cameraVisible) continue;

        // otherwise, draw them
        this->prepareChunk(program, info.wc, withNormals);
        info.wc->draw(program);
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
    if(!ImGui::Begin("ChunkLoader Overlay", &this->showsOverlay, window_flags)) {
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
    ImGui::Text("Work Queue: %5lu", chunk::ChunkWorker::getPendingItemCount());
    ImGui::SameLine();
    ImGui::Text("Culled: %lu", this->numChunksCulled);

    // context menu
    if(ImGui::BeginPopupContextWindow()) {
        if(ImGui::MenuItem("Show Draw Chunk List", nullptr, &this->showsChunkList)) {
            // nothing
        }

        ImGui::Separator();
        if(this->showsOverlay && ImGui::MenuItem("Close Overlay")) {
            this->showsOverlay = false;
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

/**
 * Draws the chunk visibility table.
 */
void ChunkLoader::drawChunkList() {
    // start window
    if(!ImGui::Begin("World Chunks", &this->showsChunkList)) {
        return;
    }

    // table
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 12);
    if(!ImGui::BeginTable("chonks", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        ImGui::End();
        return;
    }

    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 58);
    ImGui::TableSetupColumn("Ptr");
    ImGui::TableSetupColumn("Alloc");
    ImGui::TableSetupColumn("Vis", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 28);
    ImGui::TableSetupColumn("Cam", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 26);
    ImGui::TableHeadersRow();

    // iterate
    size_t allocTotal = 0;
    size_t i = 0;
    for(auto &[position, info] : this->chunks) {
        ImGui::TableNextRow();
        ImGui::PushID(i);

        ImGui::TableNextColumn();
        ImGui::Text("%d,%d", (int) position.x, (int) position.y);

        ImGui::TableNextColumn();
        ImGui::Text("%p", info.wc.get());

        ImGui::TableNextColumn();
        const auto alloc = info.wc->chunk->poolAllocSpace();
        allocTotal += alloc;

        ImGui::Text("%.4g M", (((double) alloc) / 1024. / 1024.));

        ImGui::TableNextColumn();
        ImGui::Text("%s", (this->visibilityMap.contains(position) && this->visibilityMap[position]) ? "Ye" : "No");

        ImGui::TableNextColumn();
        ImGui::Checkbox("##visible", &info.cameraVisible);

        ImGui::PopID();
        i++;
    }
    ImGui::EndTable();

    ImGui::Text("Total Row Pool Alloc: %g MBytes", ((double) allocTotal) / 1024. / 1024.);

    // finish
    ImGui::End();
}

