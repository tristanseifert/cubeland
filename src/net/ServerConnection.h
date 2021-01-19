#ifndef NET_SERVERCONNECTION_H
#define NET_SERVERCONNECTION_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>

struct tls;
struct tls_config;

namespace net {
class ServerConnection {
    public:
        /// Default server port
        constexpr static uint16_t kDefaultPort = 47420;

    public:
        ServerConnection(const std::string &host);
        ~ServerConnection();

    private:
        void buildTlsConfig(struct tls_config *);

        void workerMain();

    private:
        /// client connection, it owns the fd
        struct tls *client = nullptr;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;
};
}

#endif
