#ifndef SERVER_NET_LISTENER_H
#define SERVER_NET_LISTENER_H

#include "ListenerClient.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/socket.h>

#include <blockingconcurrentqueue.h>

struct tls;
struct tls_config;

namespace world {
class WorldSource;
}

namespace net {
/**
 * Handles opening the server's listening socket, accepting new clients, and starting the TLS
 * handshake with them.
 */
class Listener {
    friend class ListenerClient;

    public:
        Listener(world::WorldSource *world);
        ~Listener();

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
};
};

#endif
