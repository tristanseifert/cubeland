/**
 * Generates the vertex data for globules using geometry shaders and transform feedback on a
 * background work thread.
 */
#ifndef RENDER_CHUNK_VERTEXGENERATOR_H
#define RENDER_CHUNK_VERTEXGENERATOR_H

#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <variant>
#include <functional>
#include <unordered_map>
#include <utility>

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
class VertexGenerator {
    public:
        struct Buffer {
            /// number of vertices contained in the buffer
            gl::GLuint numVertices = 0;
            /// vertex buffer
            std::shared_ptr<gfx::Buffer> buffer = nullptr;
        };

        using BufList = std::vector<std::pair<glm::ivec3, Buffer>>;

        /**
         * Callback for when a globule has been yeeted. The first argument is the chunk
         * position, whereas the second is a list of globule positions (chunk relative) to vertex
         * buffers.
         */
        using Callback = std::function<void(const glm::ivec2 &, const BufList &)>;

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

    private:
        static VertexGenerator *gShared;

    private:
        struct WorkItem {
            /// time at which the work item was submitted;
            std::chrono::high_resolution_clock::time_point submitted;
            /// type of work to perform
            std::variant<std::monostate> payload;
        };

        struct CallbackInfo {
            /// world position of the chunk
            glm::ivec2 chunk;
            /// Callback function
            Callback callback;
        };

    private:
        VertexGenerator(gui::MainWindow *);
        ~VertexGenerator();

        uint32_t addCallback(const glm::ivec2 &chunkPos, const Callback &func);
        void removeCallback(const uint32_t token);

        void workerMain();

        /// Enqueues a new item to the work queue
        void submitWorkItem(WorkItem &item) {
            item.submitted = std::chrono::high_resolution_clock::now();
            this->workQueue.enqueue(item);
        }

    private:
        gui::MainWindow *window;

        void *workerGlCtx = nullptr;

        std::atomic_bool run;
        std::unique_ptr<std::thread> worker;
        moodycamel::BlockingConcurrentQueue<WorkItem> workQueue;

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
};
}

#endif
