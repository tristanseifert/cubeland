/**
 * The chunk loader handles loading chunks from the world source, and making sure they're properly
 * converted to the world chunk type that can be rendered.
 */
#ifndef RENDER_SCENE_CHUNKLOADER_H
#define RENDER_SCENE_CHUNKLOADER_H

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <future>
#include <mutex>
#include <chrono>
#include <variant>
#include <optional>
#include <functional>
#include <utility>

#include <concurrentqueue.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

#include "util/ThreadPool.h"

struct MetricsGuiMetric;
struct MetricsGuiPlot;

namespace util {
class Frustum;
}
namespace gfx {
class RenderProgram;
class Texture2D;
}
namespace world {
struct Chunk;
class ClientWorldSource;
}
namespace render {
class WorldChunk;
class SceneRenderer;
}
namespace physics {
class Engine;
}

namespace render::scene {
class ChunkLoader {
    friend class render::SceneRenderer;

    public:
        ChunkLoader();
        ~ChunkLoader();

        void setSource(std::shared_ptr<world::ClientWorldSource> source);

        void startOfFrame();
        void updateChunks(const glm::vec3 &pos, const glm::vec3 &viewDirection, const glm::mat4 &projView);
        void draw(std::shared_ptr<gfx::RenderProgram> &program, const glm::mat4 &projView, const glm::vec3 &viewDirection);

        void drawHighlights(std::shared_ptr<gfx::RenderProgram> &program, const glm::mat4 &projView);

        void setFoV(const float fov) {
            this->fov = fov;
        }

        /// Returns visibility state of the info overlay.
        const bool isOverlayShown() const {
            return this->showsOverlay;
        }
        /// Sets whether the info overlay is shown.
        void setOverlayShown(const bool show) {
            this->showsOverlay = show;
        }

        void loadPrefs();

    private:
        /// minimum amount of movement required before we do any sort of processing
        constexpr static const float kMoveThreshold = 0.1f;
        /// minimum change in direction vector to re-evaluate which chunks are visible
        constexpr static const float kDirectionThreshold = 0.02f;
        /// alpha value of the statistics overlay
        constexpr static const float kOverlayAlpha = 0.74f;
        /// bias value to add to the distance of all culled chunks when determining draw order
        constexpr static const float kCulledChunkDrawOrderDistanceBias = 42069;
        /// color for the selection of the block under the cursor
        constexpr static const glm::vec4 kLookAtSelectionColor = glm::vec4(1, 1, 0, 0.74);

    private:
        using DeferredChunk = std::future<std::shared_ptr<world::Chunk>>;
        using ChunkPtr = std::shared_ptr<world::Chunk>;

        // info describing a chunk that's finished processing
        struct LoadChunkInfo {
            // if set, the chunk is already loaded but became visible
            bool isFake = false;
            // time at which this chunk was queued
            std::chrono::high_resolution_clock::time_point queuedAt;
            // chunk's position (in chunk coordinates)
            glm::ivec2 position;
            // chunk pointer (or exception)
            std::variant<std::monostate, ChunkPtr, std::exception> data;

            // sets the queued timestamp value to the current time
            LoadChunkInfo() {
                this->queuedAt = std::chrono::high_resolution_clock::now();
            }
        };

        // info describing a chunk that's rendering
        struct RenderChunk {
            // order in which the chunks are to be drawn
            int drawOrder = -1;
            // is the chunk visible from the current camera view?
            bool cameraVisible = true;
            // actual chunk to render
            std::shared_ptr<WorldChunk> wc = nullptr;
        };

    private:
        void initDisplayChunks();

        void updateVisible(const glm::vec3 &cameraPos, const glm::mat4 &projView);

        bool updateDeferredChunks();
        void addLoadedChunk(LoadChunkInfo &pending);
        void pruneLoadedChunksList();

        void updateDrawOrder();
        void updateLookAt();
        void updateLookAtBlock();
        void removeLookAtSelection();
        void updateLookAtSelection(const glm::ivec2 chunkPos, const glm::ivec3 blockOff, const glm::mat4 &transform = glm::mat4(1));

        bool updateCenterChunk(const glm::vec3 &delta, const glm::vec3 &camera);
        void loadChunk(const glm::ivec2 position);

        void drawChunk(std::shared_ptr<gfx::RenderProgram> &program, const glm::ivec2 &pos,
                const RenderChunk &info, const bool withNormals, const util::Frustum &frustum,
                const bool special = false, const bool cull = true);
        void prepareChunk(std::shared_ptr<gfx::RenderProgram> program, 
                std::shared_ptr<WorldChunk> chunk, bool hasNormal, glm::mat4 &model);

        std::shared_ptr<WorldChunk> makeWorldChunk();

        void drawOverlay();
        void drawChunkList();
        void drawChunkMetrics();

    private:
        using WorkItem = std::function<void(void)>;

        /**
         * Data texture used to store the globule vertex normal data. This does not change at all
         * and is the same 4*6 different values for ALL vertices rendered, so we store it in a
         * texture and index into it in the shader, rather than wasting 12 bytes of vertex data.
         */
        gfx::Texture2D *globuleNormalTex = nullptr;

        /**
         * Data texture storing information needed to render each block. The Y position is used to
         * index by the block id. For the definitions of the X values, see the comments in
         * world/block/BlockDataGenerator.cpp.
         */
        gfx::Texture2D *blockInfoTex = nullptr;

        /**
         * All textures registered by blocks are combined into one texture atlas for performance
         * reasons; this texture holds this atlas.
         */
        gfx::Texture2D *blockAtlasTex = nullptr;

        /**
         * Contains material info for a particular block. This is an RGBA texture, though currently
         * only the R/G components are used: red holds the specular component, whereas G holds the
         * material shininess.
         */
        gfx::Texture2D *materialAtlasTex = nullptr;

        /**
         * Blocks can make use of normal mapping. If this is desired, they will sample out of this
         * texture atlas of normal data.
         */
        gfx::Texture2D *normalAtlasTex = nullptr;

        /**
         * Visibility map for chunks, based on current (primary) view direction
         *
         * This map is periodically updated (and garbage collected) like the loadedChunks and
         * chunks maps. It has a boolean value indicating if the most recently passed view
         * direction vector intersects with that chunk's bounding box; e.g. if any part of it is
         * visible from the current view position.
         *
         * From this data, the renderer decides whether a render chunk is allocated right away for
         * the loaded chunk or not.
         */
        std::unordered_map<glm::ivec2, bool> visibilityMap;
        /// Max distance an entry in the visibility map can be from the current position
        size_t visibilityReleaseDistance = 6;

        /**
         * Chunks that were loaded, but aren't visible are pushed here. We'll handle them when we
         * are idle (no visible chunks to deal with,) or it's been long enough.
         */
        moodycamel::ConcurrentQueue<LoadChunkInfo> loadedOffScreen;
        /**
         * Positions of all off-screen loaded chunks are put on this set; this prevents submission
         * of duplicate chunks from gumming it up.
         */
        std::unordered_set<glm::ivec2> loadedOffScreenPos;
        /**
         * Rate limiting counter for loading off-screen chunks; this counter must be decremented to
         * zero before a new chunk will be loaded.
         *
         * Every time an off-screen chunk is loaded, it's reset.
         */
        size_t eagerLoadRateLimit = 0;

#ifdef NDEBUG
        size_t eagerLoadRateLimitReset = 15;
#else
        size_t eagerLoadRateLimitReset = 30;
#endif

        /**
         * All chunk loading happens on this thread pool.
         */
        util::ThreadPool<WorkItem> chunkWorkQueue = util::ThreadPool<WorkItem>("ChunkLoader", 4);


        /**
         * Whenever a chunk load request has completed, info is pushed onto this queue. Each trip
         * through the start-of-frame handler for the loader will pop this queue until it's
         * empty and make sure those chunks are displayed.
         */
        moodycamel::ConcurrentQueue<LoadChunkInfo> loaded;
        /**
         * All chunks we've loaded.
         *
         * Periodically, we'll go through all keys in here and get rid of chunks that are more than
         * a certain distance away from the current position.
         *
         * @note May only be accessed from render thread.
         */
        std::unordered_map<glm::ivec2, std::shared_ptr<world::Chunk>> loadedChunks;
        /**
         * List of all chunks we're currently loading.
         */
        std::vector<glm::ivec2> currentlyLoading;
        /**
         * List of chunks that should be deallocated.
         *
         * When chunks go out of display range, we push them onto this queue; then, periodically,
         * a background task is queued to get rid of them. There's tons of memory management that
         * will happen here, so it helps to not block the rendering loop with that.
         */
        std::vector<std::shared_ptr<world::Chunk>> chunksToDealloc;
        std::mutex chunksToDeallocLock;

        /**
         * Defines the number of chunks that we eagerly load every time the camera moves.
         *
         * This in effect defines a "ring" of the given width around the current chunk of data that
         * is loaded.
         */
        size_t chunkRange = 3;
        /**
         * Maximum distance beyond which chunks out of the loaded chunks map will begin to be
         * unloaded to save memory.
         */
        size_t cacheReleaseDistance = 4;

        /// world source from which we get data
        std::shared_ptr<world::ClientWorldSource> source;

        /**
         * Order in which chunks should be drawn.
         *
         * Chunks are sorted from near to far, so that further away chunks don't waste time drawing
         * pixels that get overwritten by closer chunks. If this vector is empty, no particular
         * draw order is used.
         */
        std::vector<glm::ivec2> drawOrder;
        /**
         * These are the actual rendering chunks used to convert the in-memory world data to meshes
         * that are displayed.
         *
         * Every few frames we'll make sure that any chunks that are too far out of view are gotten
         * rid of and moved back to our "idle" list.
         */
        std::unordered_map<glm::ivec2, RenderChunk> chunks;
        /**
         * Spare drawing chunks; these aren't currently on screen or drawing anything, but we can
         * use them when we quickly want another chunk rather than wasting time allocating.
         *
         * The maximum number of chunks that chill in this queue is set by the `maxChunkQueueSize`
         * variable.
         */
        moodycamel::ConcurrentQueue<std::shared_ptr<WorldChunk>> chunkQueue;
        /// max size of the chunk queue
        size_t maxChunkQueueSize = 8;
        /// number of draw chunks that were culled due to visibility
        size_t numChunksCulled = 0;

        /// rate limiting timer for chunk list pruning
        size_t chunkPruneTimer = 0;
        /// reset value for the chunk prune timer
        size_t chunkPruneTimerReset = 30;

        /**
         * Pair of chunk position and selection registration id for a selection outline for the
         * selected chunk.
         */
        std::optional<std::pair<glm::ivec2, uint64_t>> lookAtSelectionMarker;

        /// chunk position of the chunk we're currently on (e.g. that the camera is on)
        glm::ivec2 centerChunkPos;
        /// chunk pos we're looking at, if any
        std::optional<glm::ivec2> lookAtChunk;
        /// block inside the look at chunk we're looking at (chunk relative coordinates)
        std::optional<glm::ivec3> lookAtBlock;
        /// chunk relative look-at block
        std::optional<glm::ivec3> lookAtBlockRelative;
        /// most recent camera position
        glm::vec3 lastPos = glm::vec3(0);
        /// most recent primary camera direction
        glm::vec3 lastDirection = glm::vec3(2, 2, 2);
        /// most recently used projection/view matrix
        glm::mat4 lastProjView = glm::mat4(1);
        /// most recently used FoV
        float fov = 70.;

        /// number of times updateChunks() has been called
        size_t numUpdates = 0;

        /// set to force updating the chunk look-at pos
        bool forceLookAtUpdate = false;
        /// set to force an update of all chunk position data
        bool forceUpdate = true;

        // data chunk allocation metrics
        MetricsGuiMetric *mAllocBytes, *mAllocDense, *mAllocSparse;
        MetricsGuiPlot *mAllocPlot;
        // various metrics for data chunks
        MetricsGuiMetric *mDataChunkLoadTime, *mDataChunks, *mDataChunksLoading, *mDataChunksPending, *mDataChunksDealloc, *mDataChunksWritePending;
        MetricsGuiPlot *mDataChunkPlot;
        // various metrics for display chunks
        MetricsGuiMetric *mDisplayChunks, *mDisplayCulled, *mDisplayEager, *mDisplayCached;
        MetricsGuiPlot *mDisplayChunkPlot;

        // pointer to physics engine (required for chunks)
        physics::Engine *physics = nullptr;

        uint32_t overlayMenuItem = 0;

        /// when set, the debug overlay is shown
#ifdef NDEBUG
        bool showsOverlay = false;
#else
        bool showsOverlay = true;
#endif
        /// when set, the visible chunks list is shown
        bool showsChunkList = false;
        /// when set, the chunk display metrics are shown
        bool showsMetrics = false;
};
}

#endif
