/**
 * Generates the vertex data for globules using geometry shaders and transform feedback on a
 * background work thread.
 */
#ifndef RENDER_CHUNK_VERTEXGENERATOR_H
#define RENDER_CHUNK_VERTEXGENERATOR_H

#include "world/chunk/Chunk.h"
#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"

#include "util/ThreadPool.h"

#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <variant>
#include <functional>
#include <unordered_map>
#include <utility>
#include <bitset>
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>

#include <glbinding/gl/types.h>
#include <blockingconcurrentqueue.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

namespace gui {
class MainWindow;
}

namespace gfx {
class Buffer;
};

namespace render::chunk {
class VertexGeneratorData;

class VertexGenerator {
    public:
        struct Buffer {
            /// number of vertices contained in the buffer
            gl::GLuint numVertices = 0;
            /// vertex buffer
            std::shared_ptr<gfx::Buffer> buffer = nullptr;

            /// number of indices, if indexed drawing shall be used
            gl::GLuint numIndices = 0;
            /// index of the first "special block," or 0 if none
            gl::GLuint specialIdxOffset = 0;
            /// bytes per index value (only 2 or 4 are allowed)
            gl::GLuint bytesPerIndex = 4;
            /// index buffer, if any
            std::shared_ptr<gfx::Buffer> indexBuffer = nullptr;
        };

        /// Vertices used to render blocks
        struct BlockVertex {
            constexpr static const uint16_t kPointFactor = 0x7F;

            /// vertex position, each value is multiplied by kPointFactor
            glm::i16vec3 p;
            gl::GLushort blockId;
            gl::GLubyte face;
            gl::GLubyte vertexId;
        };

        using BufList = std::vector<std::pair<glm::ivec3, Buffer>>;

        /**
         * Callback for when a globule has been yeeted. The first argument is the chunk
         * position, whereas the second is a list of globule positions (chunk relative) to vertex
         * buffers.
         */
        using Callback = std::function<void(const glm::ivec2 &, const BufList &)>;

        /// Mask indicating all globules are to be reprocessed
        constexpr static const uint64_t kAllGlobulesMask = 0xFFFFFFFFFFFFFFFF;

    public:
        static void init(gui::MainWindow *window);
        static void shutdown();

        /// Registers a new chunk update/completion callback
        static uint32_t registerCallback(const glm::ivec2 &chunkPos, const Callback &func) {
            return gShared->addCallback(chunkPos, func);
        }
        /// Removes an existing chunk callback.
        static void unregisterCallback(const uint32_t token) {
            /// if we've been deallocated, don't worry about callbacks since they're gone too
            if(!gShared) return;
            gShared->removeCallback(token);
        }

        /// Generates vertices for ALL globules in the given chunk
        static void update(std::shared_ptr<world::Chunk> &chunk, const bool highPriority = false) {
            gShared->generate(chunk, kAllGlobulesMask, highPriority);
        }
        /// Generates vertices for the globule with the given block offset
        static void update(std::shared_ptr<world::Chunk> &chunk, const glm::ivec3 &globulePos, const bool highPriority = false) {
            const auto bits = blockPosToBits(globulePos);
            gShared->generate(chunk, bits, highPriority);
        }
        /// Generates vertices for all globules set in the specified bit mask.
        static void update(std::shared_ptr<world::Chunk> &chunk, const uint64_t bits, const bool highPriority = false) {
            gShared->generate(chunk, bits, highPriority);
        }

        /**
         * Given a block index, returns a bitmask with the bit for the globule containing it. This
         * is organized as follows, roughly:
         *
         * YYYY-YYYY-YYYY-YYYY-YYYY-YYYY-YYYY-YYYY
         * ZZZZ-ZZZZ-ZZZZ-ZZZZ ZZZZ-ZZZZ-ZZZZ-ZZZZ
         * XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX
         *
         * Each section of letters corresponds to an X/Y/Z coordinate; those connected with dashes
         * are for segments of the same value.
         *
         * In other words, 4 bits encode the X position; this is shifted by 4*Z offset, which
         * in turn is shifted by 16*Y offset.
         */
        static inline uint64_t blockPosToBits(const glm::ivec3 &pos) {
            glm::ivec3 idx;
            world::Chunk::absoluteToRelative(pos, idx);
            idx /= glm::ivec3(64);

            uint64_t temp = 0;

            temp |= ((1 << idx.x) << (4 * idx.z)) << (16 * idx.y);

            return temp;
        }

        /// Start of frame handler
        static void startOfFrame() {
            if(!gShared) return;
            gShared->copyBuffers();
        }

    private:
        static VertexGenerator *gShared;

    private:
        using ExposureMaps = std::vector<std::array<bool, 256>>;

        /// Generation has completed and it needs to be turned into OpenGL buffers.
        struct BufferRequest {
            /// Chunk position for which the data is
            glm::ivec2 chunkPos;
            /// Globule inside that chunk for which the data is
            glm::ivec3 globuleOff;

            /// offset to the first index of the transparent drawing pass if any
            size_t specialIdxOffset = 0;

            std::variant<std::vector<gl::GLushort>, std::vector<gl::GLuint>> indices;
            std::vector<BlockVertex> vertices;
        };

        /// Request to generate globule data for the given chunk
        struct GenerateRequest {
            std::shared_ptr<world::Chunk> chunk;
            uint64_t globules;
        };

        struct WorkItem {
            /// time at which the work item was submitted;
            std::chrono::high_resolution_clock::time_point submitted;
            /// type of work to perform
            std::variant<std::monostate, GenerateRequest> payload;
        };

        struct CallbackInfo {
            /// world position of the chunk
            glm::ivec2 chunk;
            /// Callback function
            Callback callback;
        };

        /*
         * Data passed around when calculating the exposure map, as well as the block contents. It
         * contains mostly a map of which blocks are "air" at, immediately above, and below the
         * current Y level.
         */
        struct AirMap {
            std::bitset<256*256> above, current, below;
        };

    private:
        VertexGenerator(gui::MainWindow *);
        ~VertexGenerator();

        uint32_t addCallback(const glm::ivec2 &chunkPos, const Callback &func);
        void removeCallback(const uint32_t token);

        void generate(std::shared_ptr<world::Chunk> &chunk, const uint64_t globuleMask, const bool highPriority);

        void workerMain();
        void workerGenerate(const GenerateRequest &, const bool useChunkWorker);
        void workerGenerate(const std::shared_ptr<world::Chunk> &, const glm::ivec3 &, const bool highPriority = false);
        void workerGenBuffers(const BufferRequest &req);

        void buildAirMap(world::ChunkSlice *, const ExposureMaps &, std::bitset<256*256> &);
        void generateBlockIdMap(const std::shared_ptr<world::Chunk> &, ExposureMaps &);
        void flagsForBlock(const AirMap &, const size_t, const size_t, const size_t, world::Block::BlockFlags &);

        void insertCubeVertices(const AirMap &, std::vector<BlockVertex> &, std::vector<gl::GLuint> &, const size_t, const size_t, const size_t, const uint16_t);
        void insertModelVertices(const AirMap &, std::vector<BlockVertex> &, std::vector<gl::GLuint> &, const size_t, const size_t, const size_t, const uint16_t, const world::BlockRegistry::Model &);

        /// Enqueues a new item to the work queue
        void submitWorkItem(WorkItem &item) {
            item.submitted = std::chrono::high_resolution_clock::now();
            this->workQueue.enqueue(item);
        }

        void copyBuffers();

    private:
        gui::MainWindow *window;

        void *workerGlCtx = nullptr;

        std::atomic_bool run;
        std::unique_ptr<std::thread> worker;
        moodycamel::BlockingConcurrentQueue<WorkItem> workQueue;
        moodycamel::ConcurrentQueue<WorkItem> highPriorityWork;

        /// ID to use for the next globule update callback
        std::atomic_uint32_t nextCallbackId = 1;
        /**
         * Callbacks directory; this provides a mapping between an unique callback token and the
         * associated callback registration info.
         */
        std::unordered_map<uint32_t, CallbackInfo> callbacks;
        /// Lock protecting the callbacks directory
        std::mutex callbacksLock;

        /**
         * Chunk position to callback mapping. This is updated any time a callback is added or
         * removed.
         */
        std::unordered_multimap<glm::ivec2, uint32_t> chunkCallbackMap;
        /// lock protecting the 
        std::mutex chunkCallbackMapLock;

        /// maximum numbers of buffers to copy every frame
        size_t maxCopiesPerFrame = 8;
        /// buffers to be created
        moodycamel::ConcurrentQueue<BufferRequest> bufferReqs;

        using WorkFunc = std::function<void(void)>;

        /// thread pool for high priority globule updates
        util::ThreadPool<WorkFunc> highPriorityWorkQueue = util::ThreadPool<WorkFunc>("VtxGen User Update", 3);
};
}

#endif
