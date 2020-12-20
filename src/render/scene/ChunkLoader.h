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

        void updateChunks(const glm::vec3 &pos);
        void draw(std::shared_ptr<gfx::RenderProgram> program);

    private:
        void initDisplayChunks();

        void updateDeferredChunks();
        void pruneLoadedChunksList();

        bool updateCenterChunk(const glm::vec3 &delta, const glm::vec3 &camera);
        void loadChunk(const glm::ivec2 position);

        void prepareChunk(std::shared_ptr<gfx::RenderProgram> program, 
                std::shared_ptr<WorldChunk> chunk, bool hasNormal);

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

    private:
        /// minimum amount of movement required before we do any sort of processing
        constexpr static const float kMoveThreshold = 0.1f;

    private:
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
         * Defines the number of chunks that we eagerly load every time the camera moves.
         *
         * This in effect defines a "ring" of the given width around the current chunk of data that
         * is loaded.
         */
        size_t chunkRange = 2;
        /// Index of the central chunk
        size_t centerIndex;

        /// world source from which we get data
        std::shared_ptr<world::WorldSource> source;

        /**
         * These are the actual rendering chunks used to convert the in-memory world data to meshes
         * that are displayed.
         *
         * Index 0 is always the "current" chunk, e.g. the one in which the camera position is
         * located.
         */
        std::vector<std::shared_ptr<WorldChunk>> chunks;

        /// chunk position of the chunk we're currently on (e.g. that the camera is on)
        glm::ivec2 centerChunkPos;
        /// most recent camera position
        glm::vec3 lastPos = glm::vec3(0);

        /// number of times updateChunks() has been called
        size_t numUpdates = 0;
};
}

#endif
