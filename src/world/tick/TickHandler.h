/**
 * Handles processing game ticks, which happen every 25ms.
 */
#ifndef WORLD_TICK_TICKHANDLER_H
#define WORLD_TICK_TICKHANDLER_H

#include <functional>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <cstddef>

#include <concurrentqueue.h>

/// kind of yucky forward declaration
typedef int SDL_TimerID;

namespace world {
class TickHandler {
    public:
        /// Tick interval, in milliseconds
        constexpr static size_t kTickInterval = 25;

        using TickCallback = std::function<void(void)>;

    public:
        /// Initializes the tick handler.
        static void init() {
            gShared = new TickHandler;
        }
        /// Shuts down the tick handler.
        static void shutdown() {
            delete gShared;
            gShared = nullptr;
        }

        static uint32_t add(const TickCallback &cb) {
            return gShared->addCallback(cb);
        }
        static void remove(const uint32_t id) {
            gShared->removeCallback(id);
        }

        static void defer(const TickCallback &cb) {
            gShared->addDeferredWorkItem(cb);
        }

        /// perform deferred processing that needs to happen on the main thread
        static void startOfFrame() {
            gShared->doDeferredWork();
        }

    private:
        TickHandler();
        ~TickHandler();

        uint32_t addCallback(const TickCallback &cb);
        void removeCallback(const uint32_t id);

        void addDeferredWorkItem(const TickCallback &cb);

        void tick();
        void doDeferredWork();

    private:
        static uint32_t timerEntry(uint32_t interval, void *ctx) {
            static_cast<TickHandler *>(ctx)->tick();
            return interval;
        }

    private:
        static TickHandler *gShared;

    private:
        /// SDL timer id
        SDL_TimerID timer = 0;

        /// IDs for callback registrations
        uint32_t nextCallbackId = 1;
        /// callbacks to execute on each tick
        std::unordered_map<uint32_t, TickCallback> callbacks;
        /// lock over callbacks
        std::mutex callbacksLock;

        /// deferred work to do next frame
        moodycamel::ConcurrentQueue<TickCallback> deferred;
};
}

#endif
