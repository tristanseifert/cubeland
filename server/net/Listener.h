#ifndef SERVER_NET_LISTENER_H
#define SERVER_NET_LISTENER_H

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/socket.h>

struct tls;
struct tls_config;

namespace world {
class WorldReader;
}

namespace net {
class ListenerClient;

/**
 * Handles opening the server's listening socket, accepting new clients, and starting the TLS
 * handshake with them.
 */
class Listener {
    public:
        Listener(world::WorldReader *world);
        ~Listener();

    private:
        void buildTlsConfig(struct tls_config *);

        void workerMain();

    private:
        world::WorldReader *world = nullptr;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;

        /// listening socket
        int listenFd = -1;
        /// tls server struct
        struct tls *tls = nullptr;

        /// active clients
        std::vector<std::shared_ptr<ListenerClient>> clients;
        /// lock protecting clients list
        std::mutex clientLock;
};
};

#endif
