#ifndef SERVER_NET_LISTENERCLIENT_H
#define SERVER_NET_LISTENERCLIENT_H

#include <atomic>
#include <memory>
#include <thread>

#include <sys/socket.h>

struct tls;

namespace net {
class ListenerClient {
    public:
        ListenerClient(struct tls *, const int fd, const struct sockaddr_storage);
        ~ListenerClient();

    private:
        void workerMain();

    private:
        /// Client TLS connection
        struct tls *tls = nullptr;
        /// file descriptor for client
        int fd = -1;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;

        struct sockaddr_storage clientAddr;
};
};

#endif
