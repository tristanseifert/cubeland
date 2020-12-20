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

#include <concurrentqueue.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

namespace gfx {
class RenderProgram;
}
namespace world {
struct Chunk;
class WorldSource;
}
namespace render {
class WorldChunk;
}

namespace render::scene {
class ChunkLoader {
    public:
        ChunkLoader();

        void setSource(std::shared_ptr<world::WorldSource> source);

        void startOfFrame();
        void updateChunks(const glm::vec3 &pos, const glm::vec3 &viewDirection);
        void draw(std::shared_ptr<gfx::RenderProgram> program, const glm::vec3 &viewDirection);

        void setFoV(const float fov) {
            this->fov = fov;
        }

    private:
        void initDisplayChunks();

        bool updateVisible(const glm::vec3 &cameraPos);
        bool checkIntersect(const glm::vec3 &origin, const glm::vec3 &dirfrac, const glm::vec3 &lb, const glm::vec3 &rt);

        void updateDeferredChunks();
        void pruneLoadedChunksList();

        bool updateCenterChunk(const glm::vec3 &delta, const glm::vec3 &camera);
        void loadChunk(const glm::ivec2 position);

        void prepareChunk(std::shared_ptr<gfx::RenderProgram> program, 
                std::shared_ptr<WorldChunk> chunk, bool hasNormal);

        std::shared_ptr<WorldChunk> makeWorldChunk();

        void drawOverlay();
        void drawChunkList();

    private:
        using DeferredChunk = std::future<std::shared_ptr<world::Chunk>>;
        using ChunkPtr = std::shared_ptr<world::Chunk>;

        // info describing a chunk that's finished processing
        struct LoadChunkInfo {
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
            // is the chunk visible from the current camera view?
            bool cameraVisible = true;
            // actual chunk to render
            std::shared_ptr<WorldChunk> wc = nullptr;
        };

    private:
        /// minimum amount of movement required before we do any sort of processing
        constexpr static const float kMoveThreshold = 0.1f;
        /// minimum change in direction vector to re-evaluate which chunks are visible
        constexpr static const float kDirectionThreshold = 0.02f;
        /// alpha value of the statistics overlay
        constexpr static const float kOverlayAlpha = 0.42f;

    private:
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
        size_t visibilityReleaseDistance = 4;

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
        std::shared_ptr<world::WorldSource> source;

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

        /// chunk position of the chunk we're currently on (e.g. that the camera is on)
        glm::ivec2 centerChunkPos;
        /// most recent camera position
        glm::vec3 lastPos = glm::vec3(0);
        /// most recent primary camera direction
        glm::vec3 lastDirection = glm::vec3(2, 2, 2);
        /// most recently used FoV
        float fov = 70.;

        /// number of times updateChunks() has been called
        size_t numUpdates = 0;

        /// when set, the debug overlay is shown
        bool showsOverlay = true;
        /// when set, the visible chunks list is shown
        bool showsChunkList = false;
};
}

#endif
