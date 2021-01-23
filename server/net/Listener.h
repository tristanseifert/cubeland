#ifndef SERVER_NET_LISTENER_H
#define SERVER_NET_LISTENER_H

#include "ListenerClient.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/socket.h>

#include <util/ThreadPool.h>
#include <blockingconcurrentqueue.h>

#include <cpptime.h>

struct tls;
struct tls_config;

namespace world {
class WorldSource;
class Clock;
}

namespace net {
/**
 * Handles opening the server's listening socket, accepting new clients, and starting the TLS
 * handshake with them.
 */
class Listener {
    friend class ListenerClient;

    public:
        using WorkItem = std::function<void(void)>;

    public:
        Listener(world::WorldSource *world);
        ~Listener();

        util::ThreadPool<WorkItem> *getSerializerPool() {
            return this->serializerPool;
        }

        world::Clock *getClock() {
            return this->clock;
        }

        /// Registers a broadcast timer (these are repeating)
        template <class Rep, class Period>
        CppTime::timer_id addRepeatingTimer(const std::chrono::duration<Rep, Period> &when,
                const std::function<void(void)> &handler) {
            return this->timer.add(when, [=](auto) {
                handler();
            }, when);
        }
        /// unregisters a timer
        void removeTimer(const CppTime::timer_id id) {
            std::lock_guard<std::mutex> lg(this->timerLock);
            this->timer.remove(id);
        }

        /// runs a function for each client
        void forEach(const std::function<void(std::unique_ptr<ListenerClient> &)> &cb);

    protected:
        /// Marks a client for later destruction
        void removeClient(ListenerClient *rawPtr) {
            this->clientsToMurder.enqueue(rawPtr);
        }

        /// Gets the world source pointer
        world::WorldSource *getWorld() const {
            return this->world;
        }

    private:
        void buildTlsConfig(struct tls_config *);

        void workerMain();

        void murdererMain();

        void saverMain();

    private:
        world::WorldSource *world = nullptr;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;

        /// listening socket
        int listenFd = -1;
        /// tls server struct
        struct tls *tls = nullptr;

        /// active clients
        std::vector<std::unique_ptr<ListenerClient>> clients;
        /// lock protecting clients list
        std::mutex clientLock;

        /// clients to be murdered
        moodycamel::BlockingConcurrentQueue<ListenerClient *> clientsToMurder;
        /// murderization thread
        std::unique_ptr<std::thread> murderThread;

        /// thread pool for chunk serialization
        util::ThreadPool<WorkItem> *serializerPool;

        /// this thread periodically invokes the save method on connected clients
        std::unique_ptr<std::thread> saverThread;

        /// time updating
        world::Clock *clock = nullptr;

        /// shared broadcasting thymer
        CppTime::Timer timer;
        /// lock protecting the thymer (for add/remove)
        std::mutex timerLock;
};
};

#endif
