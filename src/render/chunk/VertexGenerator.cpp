#include "VertexGenerator.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"

#include "gui/MainWindow.h"
#include "io/Format.h"
#include "util/Thread.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <SDL.h>

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

    Logging::trace("Adding vtx gen callback for chunk {} (id ${:x})", chunkPos, id);
    return id;
}

/**
 * Removes a previously registered callback function.
 */
void VertexGenerator::removeCallback(const uint32_t token) {
    PROFILE_SCOPE(RemoveVtxGenCb);

    Logging::trace("Removing vtx gen callback with token ${:x}", token);

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
void VertexGenerator::generate(std::shared_ptr<world::Chunk> &chunk, const uint64_t bits) {
    Logging::trace("Generate for chunk {}: {:x}", (void *) chunk.get(), bits);
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
        {
            PROFILE_SCOPE_STR("WaitWork", 0xFF000050);
            this->workQueue.wait_dequeue(item);
        }

        const auto &p = item.payload;

        // no-op
        if(std::holds_alternative<std::monostate>(p)) {
            // nothing. duh
        }
    }

    // detach context
    SDL_GL_MakeCurrent(this->window->getSDLWindow(), nullptr);
}

