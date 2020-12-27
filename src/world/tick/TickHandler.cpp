#include "TickHandler.h"

#include <Logging.h>
#include <mutils/time/profiler.h>

#include <SDL.h>

using namespace world;

TickHandler *TickHandler::gShared = nullptr;


/**
 * Initializes the tick handler. This sets up the SDL timer that actually handles ticking.
 */
TickHandler::TickHandler() {
    this->timer = SDL_AddTimer(kTickInterval, &TickHandler::timerEntry, this);
    XASSERT(this->timer, "Failed to create tick timer");
}

/**
 * Stops the tick timer and any pending ticks.
 */
TickHandler::~TickHandler() {
    bool success = SDL_RemoveTimer(this->timer);
    XASSERT(success, "Failed to remove tick timer");
}

/**
 * Tick callback
 */
void TickHandler::tick() {
    PROFILE_SCOPE(Tick);

    // invoke callbacks
    for(const auto &[id, callback] : this->callbacks) {
        callback();
    }
}

/**
 * Installs a new tick callback.
 */
uint32_t TickHandler::addCallback(const TickCallback &cb) {
    uint32_t nextId = this->nextCallbackId++;

    this->callbacks[nextId] = cb;
    return nextId;
}

/**
 * Removes an existing tick callback.
 */
void TickHandler::removeCallback(const uint32_t id) {
    this->callbacks.erase(id);
}


/**
 * Pushes the given item to the deferred work queue.
 */
void TickHandler::addDeferredWorkItem(const TickCallback &cb) {
    this->deferred.enqueue(cb);
}

/**
 * Invokes any deferred main thread work.
 */
void TickHandler::doDeferredWork() {
    PROFILE_SCOPE(DeferredTicks);

    TickCallback cb;
    while(this->deferred.try_dequeue(cb)) {
        cb();
    }
}
