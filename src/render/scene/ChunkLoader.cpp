#include "ChunkLoader.h"

#include "render/chunk/WorldChunk.h"
#include "render/chunk/Globule.h"
#include "render/chunk/ChunkWorker.h"
#include "world/chunk/Chunk.h"
#include "world/block/BlockRegistry.h"
#include "world/WorldSource.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"
#include "util/Frustum.h"
#include "util/Intersect.h"
#include "io/MetricsManager.h"
#include "io/Format.h"
#include <Logging.h>
#include <mutils/time/profiler.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <imgui.h>
#include <metricsgui/metrics_gui.h>

#include <algorithm>

using namespace world;
using namespace render::scene;

/**
 * Initializes the chunk loader.
 */
ChunkLoader::ChunkLoader() {
    // allocate the face ID -> normal coordinate texture
    this->globuleNormalTex = new gfx::Texture2D(0);
    this->globuleNormalTex->setUsesLinearFiltering(false);
    this->globuleNormalTex->setDebugName("ChunkNormalMap");

    chunk::Globule::fillNormalTex(this->globuleNormalTex);

    // allocate the block texture atlas
    this->blockAtlasTex = new gfx::Texture2D(1);
    this->blockAtlasTex->setUsesLinearFiltering(false);
    this->blockAtlasTex->setDebugName("ChunkBlockAtlas");

    {
        glm::ivec2 atlasSize;
        std::vector<std::byte> data;

        BlockRegistry::generateBlockTextureAtlas(atlasSize, data);

        this->blockAtlasTex->allocateBlank(atlasSize.x, atlasSize.y, gfx::Texture2D::RGBA16F);
        this->blockAtlasTex->bufferSubData(atlasSize.x, atlasSize.y, 0, 0,  gfx::Texture2D::RGBA16F, data.data());
    }

    this->blockAtlasTex->bind();
    gl::glGenerateMipmap(gl::GL_TEXTURE_2D);
    gl::glTexParameterf(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.f);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR_MIPMAP_LINEAR);

    // allocate a texture holding block ID info data
    this->blockInfoTex = new gfx::Texture2D(2);
    this->blockInfoTex->setUsesLinearFiltering(false);
    this->blockInfoTex->setDebugName("ChunkBlockInfo");

    {
        glm::ivec2 dataTexSize;
        std::vector<glm::vec4> data;

        BlockRegistry::generateBlockData(dataTexSize, data);

        // TODO: investigate why a 2 component format does NOT work
        this->blockInfoTex->allocateBlank(dataTexSize.x, dataTexSize.y, gfx::Texture2D::RGBA16F);
        this->blockInfoTex->bufferSubData(dataTexSize.x, dataTexSize.y, 0, 0,  gfx::Texture2D::RGBA16F, data.data());
    }


    // set up chunk collection
    this->initDisplayChunks();

    // allocator metrics
    this->mAllocBytes = new MetricsGuiMetric("Allocated Memory", "B", MetricsGuiMetric::USE_SI_UNIT_PREFIX);
    this->mAllocSparse = new MetricsGuiMetric("Sparse Alloc", "segments", 0);
    this->mAllocDense = new MetricsGuiMetric("Dense Alloc", "segments", 0);

    this->mAllocPlot = new MetricsGuiPlot;
    this->mAllocPlot->mInlinePlotRowCount = 3;
    this->mAllocPlot->mShowInlineGraphs = true;
    this->mAllocPlot->mShowAverage = true;
    this->mAllocPlot->mShowLegendUnits = false;

    this->mAllocPlot->AddMetric(this->mAllocBytes);
    this->mAllocPlot->AddMetric(this->mAllocSparse);
    this->mAllocPlot->AddMetric(this->mAllocDense);

    // set up data chunk metrics
    this->mDataChunkLoadTime = new MetricsGuiMetric("Load Time", "s", MetricsGuiMetric::USE_SI_UNIT_PREFIX);
    this->mDataChunks = new MetricsGuiMetric("Cached", "chunks", 0);
    this->mDataChunksLoading = new MetricsGuiMetric("Loading", "chunks", 0);
    this->mDataChunksPending = new MetricsGuiMetric("Pending Process", "chunks", 0);
    this->mDataChunksDealloc = new MetricsGuiMetric("Pending Dealloc", "chunks", 0);
    this->mDataChunksWritePending = new MetricsGuiMetric("Pending Write", "chunks", 0);

    this->mDataChunkPlot = new MetricsGuiPlot;
    this->mDataChunkPlot->mInlinePlotRowCount = 3;
    this->mDataChunkPlot->mShowInlineGraphs = true;
    this->mDataChunkPlot->mShowAverage = true;
    this->mDataChunkPlot->mShowLegendUnits = false;

    this->mDataChunkPlot->AddMetric(this->mDataChunkLoadTime);
    this->mDataChunkPlot->AddMetric(this->mDataChunks);
    this->mDataChunkPlot->AddMetric(this->mDataChunksLoading);
    this->mDataChunkPlot->AddMetric(this->mDataChunksPending);
    this->mDataChunkPlot->AddMetric(this->mDataChunksDealloc);
    this->mDataChunkPlot->AddMetric(this->mDataChunksWritePending);

    // set up the display chunk metrics
    this->mDisplayChunks = new MetricsGuiMetric("Allocated", "chunks", 0);
    this->mDisplayCulled = new MetricsGuiMetric("Culled", "chunks", 0);
    this->mDisplayEager = new MetricsGuiMetric("Eager Load Pending", "chunks", 0);
    this->mDisplayCached = new MetricsGuiMetric("Obj Cached", "chunks", 0);

    this->mDisplayChunkPlot = new MetricsGuiPlot;
    this->mDisplayChunkPlot->mInlinePlotRowCount = 3;
    this->mDisplayChunkPlot->mShowInlineGraphs = true;
    this->mDisplayChunkPlot->mShowAverage = true;
    this->mDisplayChunkPlot->mShowLegendUnits = false;

    this->mDisplayChunkPlot->AddMetric(this->mDisplayChunks);
    this->mDisplayChunkPlot->AddMetric(this->mDisplayCulled);
    this->mDisplayChunkPlot->AddMetric(this->mDisplayEager);
    this->mDisplayChunkPlot->AddMetric(this->mDisplayCached);
}

/**
 * Cleans up all the resources of the chunk loader.
 */
ChunkLoader::~ChunkLoader() {
    delete this->mAllocPlot;
    delete this->mDataChunkPlot;
    delete this->mDisplayChunkPlot;

    delete this->mAllocBytes;
    delete this->mAllocDense;
    delete this->mAllocSparse;
    delete this->mDataChunkLoadTime;
    delete this->mDataChunks;
    delete this->mDataChunksLoading;
    delete this->mDataChunksPending;
    delete this->mDataChunksDealloc;
    delete this->mDataChunksWritePending;
    delete this->mDisplayChunks;
    delete this->mDisplayCulled;
    delete this->mDisplayEager;
    delete this->mDisplayCached;

    delete this->globuleNormalTex;
    delete this->blockInfoTex;
    delete this->blockAtlasTex;
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
    if(!this->chunkPruneTimer){
        this->pruneLoadedChunksList();

        this->chunkPruneTimer = this->chunkPruneTimerReset;
    } else {
        this->chunkPruneTimer--;
    }

    // collect world source statistics
    this->mDataChunksWritePending->AddNewValue(this->source->numPendingWrites());

    // draw the overlay if enabled
    if(this->showsOverlay) {
        this->drawOverlay();
    }
    if(this->showsChunkList) {
        this->drawChunkList();
    }
    if(this->showsMetrics) {
        this->drawChunkMetrics();
    }
}

/**
 * Called at the start of a frame, this checks to see if we need to load any additional chunks as
 * the player moves.
 */
void ChunkLoader::updateChunks(const glm::vec3 &pos, const glm::vec3 &viewDirection, const glm::mat4 &projView) {
    bool visibilityChanged = this->forceUpdate;
    bool needsDrawOrderUpdate = this->forceLookAtUpdate | this->forceUpdate;

    // perform any deferred chunk loading/unloading
    this->updateVisible(pos, projView);
    this->updateDeferredChunks();

    // update which chunks are visible at the moment only if the direction changed enough
    glm::vec3 dirDelta = viewDirection - this->lastDirection;
    if(!glm::all(glm::epsilonEqual(dirDelta, glm::vec3(0), kDirectionThreshold))) {
        this->lastDirection = viewDirection;

        visibilityChanged = true;
    }

    // if position update was significant, update both it AND the visibility map
    glm::vec3 delta = pos - this->lastPos;
    if(!glm::all(glm::epsilonEqual(delta, glm::vec3(0), kMoveThreshold))) {
        this->lastPos = pos;

        visibilityChanged = true;
    }

    // handle the current central chunk
    if(this->updateCenterChunk(delta, pos) || visibilityChanged) {
        needsDrawOrderUpdate = true;

        // recalculate all the surrounding chunks
        for(int xOff = -this->chunkRange; xOff <= (int)this->chunkRange; xOff++) {
            for(int zOff = -this->chunkRange; zOff <= (int)this->chunkRange; zOff++) {
                const auto chunkPos = this->centerChunkPos + glm::ivec2(xOff, zOff);

                // if x == z == 0, it's the central chunk and we can skip it
                if(!xOff && !zOff) continue;

                // request the chunk
                this->loadChunk(chunkPos);
            }
        }
    }

    // update draw order if needed
    if(needsDrawOrderUpdate) {
        this->updateDrawOrder();
    }
    if(needsDrawOrderUpdate || this->forceLookAtUpdate) {
        this->updateLookAt();
        this->forceLookAtUpdate = false;
    }

    // update all chunks
    for(auto [position, info] : this->chunks) {
        if(info.wc) {
            info.wc->frameBegin();
        }
    }

    this->forceUpdate = false;
    this->numUpdates++;
}

/**
 * Updates the visibility map from the current camera perspective using frustum culling.
 */
void ChunkLoader::updateVisible(const glm::vec3 &cameraPos, const glm::mat4 &projView) {
    PROFILE_SCOPE(UpdateVisibleChunks);

    // createa frustum
    util::Frustum frust(projView);

    // cheack each chunk
    const glm::vec2 centerChunk = glm::vec2(cameraPos.x/256., cameraPos.z/256.) * glm::vec2(256., 256.);

    for(int xOff = -this->chunkRange; xOff <= (int)this->chunkRange; xOff++) {
        for(int zOff = -this->chunkRange; zOff <= (int)this->chunkRange; zOff++) {
            // calculate its bounding rect
            const glm::vec2 chunkPos = centerChunk + glm::vec2(xOff * 256., zOff * 256.);
            const glm::vec3 lb(chunkPos.x, 0, chunkPos.y), rt(chunkPos.x+256., 256, chunkPos.y+256.);

            const bool intersects = frust.isBoxVisible(lb, rt);
            const auto key = this->centerChunkPos + glm::ivec2(xOff, zOff);
            this->visibilityMap.insert_or_assign(key, intersects);
        }
    }
}

/**
 * Updates the "look at" chunk. This is determined by casting a ray from the camera's current
 * position and direction and seeing with which chunk it intersects, if any. We iterate them in
 * draw order in hopes of speeding things up.
 *
 * Note that this is limited to currently loaded display chunks.
 */
void ChunkLoader::updateLookAt() {
    using namespace util;
    PROFILE_SCOPE(UpdateLookAt);

    // calculate ray
    glm::vec3 dirfrac = glm::vec3(1., 1., 1.) / this->lastDirection;

    // iterate all visible chunks
    for(const auto chunkPos : this->drawOrder) {
        // build the min and max corners of the chunk
        glm::vec2 worldOrigin(chunkPos.x * 256., chunkPos.y * 256.);
        glm::vec3 min(worldOrigin.x, 0, worldOrigin.y);
        glm::vec3 max = min + glm::vec3(255, 255, 255);

        if(Intersect::rayArbb(this->lastPos, dirfrac, min, max)) {
            this->lookAtChunk = chunkPos;
            this->updateLookAtBlock();
            return;
        }
    }

    // if we get down here, not looking at any chonks
    this->lookAtChunk.reset();
    this->lookAtBlock.reset();
    this->lookAtBlockRelative.reset();
}

/**
 * Determines what block we're looking at.
 *
 * Note that this may be in a different chunk than the one we're looking at; this is the case if
 * we're standing very close to a chunk boundary.
 *
 * This works by casting a ray from the current look-at position to large bounding boxes to first
 * determine if we should test chunks in the +X or -X direction, as well as the +Z or -Z direction;
 * once this has been established, we try all blocks in a 6x6 range, and record the distance for
 * those that pass the intersection test.
 *
 * Iterating over the list of touched blocks in increasing distance order, we check the block at
 * that location. If it's not air, mark it as the pointed-at block.
 */
void ChunkLoader::updateLookAtBlock() {
    using namespace util;
    PROFILE_SCOPE(UpdateLookAtBlock);

    const int kPointingDistance = 6; // max distance we check for selection

    // calculate ray
    glm::vec3 dirfrac = glm::vec3(1., 1., 1.) / this->lastDirection;

    // check all intersecting blocks within a 6 block distance
    std::vector<std::pair<glm::ivec3, float>> distances;
    const glm::vec3 lbo = glm::floor(this->lastPos), rto = lbo + glm::vec3(1., 1., 1.);

    for(int z = -kPointingDistance; z <= kPointingDistance; z++) {
        for(int y = -kPointingDistance; y <= kPointingDistance; y++) {
            for(int x = -kPointingDistance; x <= kPointingDistance; x++) {
                const auto lb = lbo + glm::vec3(x, y, z), rt = rto + glm::vec3(x, y, z);

                if(Intersect::rayArbb(this->lastPos, dirfrac, lb, rt)) {
                    const auto middle = lb + glm::vec3(.5, .5, .5);
                    const auto dist = glm::distance(middle, this->lastPos);

                    if(dist <= kPointingDistance) {
                        distances.emplace_back(glm::ivec3(lb), dist);
                    }
                }
            }
        }
    }

    // sort by distance
    std::sort(std::begin(distances), std::end(distances), [](const auto &l, const auto &r) {
        return (l.second < r.second);
    });

    // try to find the first non-air block
    for(const auto [blockPos, distance] : distances) {
        // convert the block position to a chunk position and try to get the chunk
        glm::vec2 chunkPosF = glm::vec2(blockPos.x, blockPos.z) / glm::vec2(256., 256.);
        glm::ivec2 chunkPos = glm::ivec2(glm::floor(chunkPosF));

        if(!this->loadedChunks.contains(chunkPos)) {
            continue;
        }

        auto chunk = this->loadedChunks[chunkPos];

        // read the 8-bit ID of the block at this position
        int zOff = (blockPos.z % 256), xOff = (blockPos.x % 256);
        if(zOff < 0) {
            zOff = 256 - abs(zOff);
        } if(xOff < 0) {
            xOff = 256 - abs(xOff);
        }

        auto slice = chunk->slices[blockPos.y];
        if(!slice) continue;
        auto row = slice->rows[zOff];
        if(!row) continue;

        const auto temp = row->at(xOff);
        const auto &map = chunk->sliceIdMaps[row->typeMap];
        const auto uuid = map.idMap[temp];

        if(!BlockRegistry::isAirBlock(uuid)) {
            // it is not air, we've found the block we're looking at
            this->lookAtBlock = glm::ivec3(blockPos);
            this->lookAtBlockRelative = glm::ivec3(xOff, blockPos.y, zOff);
            //Logging::trace("Look@ block {} (off {}, {}) chunk {}, dist {}, id {}", blockPos, xOff,
            //        zOff, chunkPos, distance, uuids::to_string(uuid));

            // update selection
            this->updateLookAtSelection(chunkPos, glm::ivec3(xOff, blockPos.y, zOff));
            return;
        }
    }

    // if we get here, failed to find a non-air block
    this->removeLookAtSelection();
    this->lookAtBlock.reset();
    this->lookAtBlockRelative.reset();
}

/**
 * Removes the current selection.
 */
void ChunkLoader::removeLookAtSelection() {
    // ensure we even have a selection
    if(!this->lookAtSelectionMarker) return;

    // try to get the chunk
    auto pos = this->lookAtSelectionMarker->first;
    if(!this->chunks.contains(pos)) {
        this->lookAtSelectionMarker.reset();
        return;
    }

    auto info = this->chunks[pos];
    auto chunk = info.wc;
    if(!chunk) goto beach;

    // unregister
    chunk->removeHighlight(this->lookAtSelectionMarker->second);

beach:;
    // clear the selection variable
    this->lookAtSelectionMarker.reset();
}

/**
 * Updates the selection of the look-at block.
 *
 * Assuemes that the chunk for the block is loaded.
 */
void ChunkLoader::updateLookAtSelection(const glm::ivec2 chunkPos, const glm::ivec3 blockOff) {
    // remove old selection
    this->removeLookAtSelection();

    // get the world chunk
    if(!this->chunks.contains(chunkPos)) return;
    auto &chunkInfo = this->chunks[chunkPos];
    if(!chunkInfo.wc) return;
    auto chunk = chunkInfo.wc;

    // build selection range
    const glm::vec3 start = glm::vec3(blockOff), end = start + glm::vec3(1.);

    auto id = chunk->addHighlight(start, end, kLookAtSelectionColor);
    this->lookAtSelectionMarker = std::make_pair(chunkPos, id);
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
    bool addedDisplayChunk = false;

    std::vector<std::pair<LoadChunkInfo, float>> toDisplay;

    // get stats on the size of the loaded chunks queue
    this->mDataChunksPending->AddNewValue(this->loaded.size_approx());

    // get all finished chunks
    LoadChunkInfo pending;
    while(this->loaded.try_dequeue(pending)) {
        using namespace std::chrono;

        // get how long it took
        const auto now = high_resolution_clock::now();
        const auto diff = now - pending.queuedAt;
        const auto diffUs = duration_cast<microseconds>(diff).count();

        if(!pending.isFake) {
            this->mDataChunkLoadTime->AddNewValue(diffUs / 1000. / 1000.);
        }

        // contains chunk data?
        if(std::holds_alternative<ChunkPtr>(pending.data)) {
            // skip assigning it to a draw chunk if it is not currently visible
            if(this->visibilityMap.contains(pending.position) && this->visibilityMap[pending.position]) {
                // calculate distance and store it
                glm::vec3 chunkOrigin(128. + (pending.position.x * 256.), 128.,
                        128. + (pending.position.y * 256.));
                auto dist = glm::distance(this->lastPos, chunkOrigin);

                toDisplay.emplace_back(pending, dist);
            }
            // if it's off screen, and not rendered, deal with it later
            else if(!this->chunks.contains(pending.position) && 
                    !this->loadedOffScreenPos.contains(pending.position)) {
                this->loadedOffScreen.enqueue(pending);
                this->loadedOffScreenPos.insert(pending.position);
            }

            // regardless, store the chunk in the cache
            this->loadedChunks[pending.position] = std::get<ChunkPtr>(pending.data);
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

    // sort all chunks to display by distance, then create in increasing distance order
    if(!toDisplay.empty()) {
        std::sort(std::begin(toDisplay), std::end(toDisplay), [](const auto &l, const auto &r) {
            return (l.second < r.second);
        });

        // iterate over them to create; this will be in INCREASING distance from us
        for(auto &pair : toDisplay) {
            this->addLoadedChunk(pair.first);
        }

        // reset the eager loading timer
        if(this->eagerLoadRateLimit < this->eagerLoadRateLimitReset) {
            this->eagerLoadRateLimit = this->eagerLoadRateLimitReset;
        } else {
            this->eagerLoadRateLimit += this->eagerLoadRateLimitReset;
            this->eagerLoadRateLimit = std::min(this->eagerLoadRateLimit, 5 * this->eagerLoadRateLimitReset);
        }

        addedDisplayChunk = true;
    }

    // if no display chunks were added this frame, create our idle chunk if timer permits
    if(!addedDisplayChunk && (!this->eagerLoadRateLimit || !--this->eagerLoadRateLimit)) {
again:;
        if(this->loadedOffScreen.try_dequeue(pending)) {
            // if this chunk is _way_ off screen, ignore it and check another one
            glm::vec3 chunkOrigin(128. + (pending.position.x * 256.), 128.,
                    128. + (pending.position.y * 256.));
            const auto dist = glm::distance(chunkOrigin, this->lastPos);
            if(dist > (this->chunkRange * 256. * 1.5)) {
                goto again;
            }

            // otherwise, process it
            this->addLoadedChunk(pending);
            this->loadedOffScreenPos.insert(pending.position);

            this->forceUpdate = true;
            this->eagerLoadRateLimit = this->eagerLoadRateLimitReset;
        }
    }


    this->mDisplayEager->AddNewValue(this->loadedOffScreen.size_approx());
}

/**
 * Adds the given chunk to a display chunk so that it can be shown.
 */
void ChunkLoader::addLoadedChunk(LoadChunkInfo &pending) {
    auto chunk = std::get<ChunkPtr>(pending.data);

    // update existing chunk
    if(this->chunks.contains(pending.position)) {
        // TODO: what do
        // this->chunks[pending.position].wc->setChunk(chunk);
    }
    // otherwise, create a new one, immediately if it's visible
    else {
        auto wc = this->makeWorldChunk();
        wc->setChunk(chunk);

        RenderChunk info;
        info.wc = wc;

        this->chunks[pending.position] = info;
        this->forceLookAtUpdate = true;

        // invoke block handlers
        BlockRegistry::notifyChunkLoaded(chunk);
    }
}

/**
 * Either pops an previous chunk off the chunk queue, or allocates a new one.
 */
std::shared_ptr<render::WorldChunk> ChunkLoader::makeWorldChunk() {
    std::shared_ptr<WorldChunk> chunk = nullptr;

    // get one from the queue if possible
    if(this->chunkQueue.try_dequeue(chunk)) {
        this->mDisplayCached->AddNewValue(this->chunkQueue.size_approx());
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

        this->mDataChunksDealloc->AddNewValue(this->chunksToDealloc.size());
        this->chunksToDeallocLock.unlock();
    }

    // then, the drawing chunks
    {
        PROFILE_SCOPE(DrawChunk);
        std::vector<glm::ivec2> toRemove;
        for(const auto &[pos, info] : this->chunks) {
            const auto distance = std::max(fabs(pos.x - this->centerChunkPos.x), fabs(pos.y - this->centerChunkPos.y));
            if(distance > this->chunkRange) {
                if(info.wc) {
                    info.wc->setChunk(nullptr);

                    // TODO: check we won't overfill the queue lol
                    this->chunkQueue.enqueue(info.wc);
                }
                toRemove.push_back(pos);
            }
        }

        for(const auto &pos : toRemove) {
            this->chunks.erase(pos);
        }
        numWorldChunks = toRemove.size();

        this->mDisplayCached->AddNewValue(this->chunkQueue.size_approx());
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
        auto future = this->chunkWorkQueue.queueWorkItem([&] {
            PROFILE_SCOPE(DeallocChunks);
            LOCK_GUARD(this->chunksToDeallocLock, DeallocChunksList);

            // invoke unload handler and ensure chunk is written out if dirty
            for(auto &chunk : this->chunksToDealloc) {
                BlockRegistry::notifyChunkWillUnload(chunk);
                this->source->forceChunkWriteSync(chunk);
            }

            this->chunksToDealloc.clear();
        });
    }
}

/**
 * Updates the draw order of the chunks.
 *
 * Chunks are ordered by their distance from the current position.
 */
void ChunkLoader::updateDrawOrder() {
    PROFILE_SCOPE(UpdateDrawOrder);

    std::vector<std::pair<glm::ivec2, float>> drawOrder;
    drawOrder.reserve(this->chunks.size());

    // build a distance map for all chunks
    {
        PROFILE_SCOPE(BuildDistMap);

        for(const auto &[pos, info] : this->chunks) {
            glm::vec3 chunkOrigin(128. + (pos.x * 256.), 128., 128. + (pos.y * 256.));
            auto dist = glm::distance(this->lastPos, chunkOrigin);

            // apply bias if the chunk is not visible
            if(this->visibilityMap.contains(pos) && !this->visibilityMap[pos]) {
                dist += kCulledChunkDrawOrderDistanceBias;
            }

            drawOrder.emplace_back(pos, dist);
        }
    }

    // sort it
    {
        PROFILE_SCOPE(SortMap);

        std::sort(std::begin(drawOrder), std::end(drawOrder), [](const auto &l, const auto &r) {
            return (l.second < r.second);
        });
    }

    // get just the first element and we've got the new map
    {
        PROFILE_SCOPE(ConvertMap);
        this->drawOrder.clear();

        int i = 0;
        for(const auto &[pos, dist] : drawOrder) {
            this->chunks[pos].drawOrder = i++;
            this->drawOrder.push_back(pos);
        }
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
        info.isFake = true;
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
    auto future = this->chunkWorkQueue.queueWorkItem([&, position] {
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
void ChunkLoader::draw(std::shared_ptr<gfx::RenderProgram> &program, const glm::mat4 &projView, const glm::vec3 &viewDirection) {
    const bool withNormals = program->rendersColor();

    // bind data textures/buffers
    if(program->rendersColor()) {
        this->blockInfoTex->bind();
        program->setUniform1i("blockTypeDataTex", this->blockInfoTex->unit);

        this->globuleNormalTex->bind();
        program->setUniform1i("vtxNormalTex", this->globuleNormalTex->unit);

        this->blockAtlasTex->bind();
        program->setUniform1i("blockTexAtlas", this->blockAtlasTex->unit);

        this->numChunksCulled = 0;
        this->lastProjView = projView;
    }

    // set up frustum for culling
    util::Frustum frust(projView);

    // use the draw order if we have one
    if(!this->drawOrder.empty()) {
        for(const auto &pos : this->drawOrder) {
            const auto &info = this->chunks[pos];
            this->drawChunk(program, pos, info, withNormals, frust);
        }
    }
    // otherwise, draw them in iteration order
    else {
        for(auto [pos, info] : this->chunks) {
            this->drawChunk(program, pos, info, withNormals, frust);
        }
    }

    // update counts
    if(program->rendersColor()) {
        this->mDataChunks->AddNewValue(this->loadedChunks.size());
        this->mDataChunksLoading->AddNewValue(this->currentlyLoading.size());

        this->mDisplayChunks->AddNewValue(this->chunks.size()); 
        this->mDisplayCulled->AddNewValue(this->numChunksCulled);
    }
}

/**
 * Draws a single chunk.
 */
void ChunkLoader::drawChunk(std::shared_ptr<gfx::RenderProgram> &program, const glm::ivec2 &pos,
        const RenderChunk &info, const bool withNormals, const util::Frustum &frustum,
        const bool cull) {
    // ignore chunks without any data or if they're invisible
    if(!info.wc || !info.wc->chunk) return;
    else if(!info.cameraVisible) return;

    // get the model matrix
    glm::mat4 model(1);
    this->prepareChunk(program, info.wc, withNormals, model);

    // skip if not culling
    if(!cull) {
        goto draw;
    }
    // skip ahead if it's the current chunk
    if(pos == this->centerChunkPos) {
        goto draw;
    }

    // check the frustum
    {
        PROFILE_SCOPE(CheckFustrum);

        // build the lower and upper edges
        const glm::vec3 chunkOrigin((pos.x * 256.), 0, (pos.y * 256.));

        const auto min = chunkOrigin, max = chunkOrigin + glm::vec3(256.);
        const bool intersects = frustum.isBoxVisible(min, max);

        if(intersects) {
            goto draw;
        }
    }

    // if we get here, all tested points were out of the view
    goto cull;

draw:;
    // otherwise, draw them
    info.wc->draw(program);
    return;

cull:;
    // do not draw anything
    if(withNormals) {
        this->numChunksCulled++;
    }
    return;
}

/**
 * Prepares a chunk for drawing.
 */
void ChunkLoader::prepareChunk(std::shared_ptr<gfx::RenderProgram> program,
        std::shared_ptr<WorldChunk> chunk, bool hasNormal, glm::mat4 &model) {
    auto &c = chunk->chunk;

    // translate based on the chunk's origin
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
 * Draws highlights.
 */
void ChunkLoader::drawHighlights(std::shared_ptr<gfx::RenderProgram> &program, const glm::mat4 &projView) {
    PROFILE_SCOPE(ChunkHighlights);

    for(auto [pos, info] : this->chunks) {
        auto chunk = info.wc;
        if(!chunk) continue;

        if(chunk->needsDrawHighlights()) {
            glm::mat4 model(1.);
            this->prepareChunk(program, chunk, false, model);
            chunk->drawHighlights(program);
        }
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

    ImGui::SetNextWindowSize(ImVec2(250, 0));
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

    ImGui::SetNextWindowBgAlpha(kOverlayAlpha);
    if(!ImGui::Begin("ChunkLoader Overlay", &this->showsOverlay, window_flags)) {
        return;
    }

    // current camera and chunk positions
    ImGui::Text("Chunk: %d, %d", this->centerChunkPos.x, this->centerChunkPos.y);
    ImGui::SameLine();
    ImGui::Text("Camera: %.1f, %.1f, %.1f", this->lastPos.x, this->lastPos.y, this->lastPos.z);

    // look at chunk and block
    if(this->lookAtChunk) {
        ImGui::Text("LookAt: %d, %d", this->lookAtChunk->x, this->lookAtChunk->y);
    } else {
        ImGui::TextUnformatted("LookAt: C (null)");
    }
    ImGui::SameLine();
    if(this->lookAtBlock) {
        ImGui::Text("B %d, %d, %d", this->lookAtBlock->x, this->lookAtBlock->y,
                this->lookAtBlock->z);
    } else {
        ImGui::TextUnformatted("B (null)");
    }

    // active chunks (data and drawing)
    ImGui::Text("Count: %lu data (%lu pend), %lu draw (%lu cache)", this->loadedChunks.size(),
            this->currentlyLoading.size(), this->chunks.size(), this->chunkQueue.size_approx());

    // chunk work queue items remaining
    ImGui::Text("Work Queue: C %5lu L %5lu", chunk::ChunkWorker::getPendingItemCount(),
            this->chunkWorkQueue.numPending());
    ImGui::SameLine();
    ImGui::Text("Culled: %lu", this->numChunksCulled);

    ImGui::Text("Offscreen Draw Pend: %lu (%lu ticks)", this->loadedOffScreen.size_approx(),
            this->eagerLoadRateLimit);

    // context menu
    if(ImGui::BeginPopupContextWindow()) {
        ImGui::MenuItem("Show Draw Chunk List", nullptr, &this->showsChunkList);
        ImGui::MenuItem("Show Chunk Metrics", nullptr, &this->showsMetrics);

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
    if(!ImGui::BeginTable("chonks", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        ImGui::End();
        return;
    }

    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 58);
    ImGui::TableSetupColumn("Ptr");
    ImGui::TableSetupColumn("Alloc");
    ImGui::TableSetupColumn("Ord", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 28);
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

        const auto denseAlloc = info.wc->chunk->poolDense.storage.size();
        const auto sparseAlloc = info.wc->chunk->poolSparse.storage.size();

        ImGui::Text("%.4g M", (((double) alloc) / 1024. / 1024.));
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Pool alloc: %lu bytes\nDense pool: %lu (%lu bytes)\nSparse pool: %lu (%lu bytes)", 
                    alloc, denseAlloc, denseAlloc * sizeof(Chunk::Pool<ChunkSliceRowDense>::Storage), 
                    sparseAlloc, sparseAlloc * sizeof(Chunk::Pool<ChunkSliceRowSparse>::Storage));
        }

        ImGui::TableNextColumn();
        ImGui::Text("%d", info.drawOrder);

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

/**
 * Draws the chunk metrics window.
 */
void ChunkLoader::drawChunkMetrics() {
    // start window
    if(!ImGui::Begin("Chunks Metrics", &this->showsChunkList)) {
        return;
    }

    // calculate the alloc totals
    size_t allocTotal = 0, allocDense = 0, allocSparse = 0;
    for(auto &[position, info] : this->chunks) {
        // skip chunks that have become deallocated
        if(!info.wc || !info.wc->chunk) continue;

        // alloc totals
        allocTotal += info.wc->chunk->poolAllocSpace();

        // per type totals
        allocDense += info.wc->chunk->poolDense.storage.size();
        allocSparse += info.wc->chunk->poolSparse.storage.size();
    }

    this->mAllocBytes->AddNewValue(allocTotal);
    this->mAllocDense->AddNewValue(allocDense);
    this->mAllocSparse->AddNewValue(allocSparse);

    // update the axes for each
    this->mAllocPlot->UpdateAxes();
    this->mDataChunkPlot->UpdateAxes();
    this->mDisplayChunkPlot->UpdateAxes();

    // allocator
    if(ImGui::CollapsingHeader("Data Memory")) {
        this->mAllocPlot->DrawList();
    }

    // data chunks
    if(ImGui::CollapsingHeader("Data Chunks")) {
        this->mDataChunkPlot->DrawList();
    }

    // display chunks
    if(ImGui::CollapsingHeader("Display Chunks")) {
        this->mDisplayChunkPlot->DrawList();
    }

    // finish
    ImGui::End();
}
