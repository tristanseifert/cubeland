#ifndef SERVER_NET_LISTENERCLIENT_H
#define SERVER_NET_LISTENERCLIENT_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>

#include <sys/socket.h>

struct tls;

namespace net {
struct PacketHeader;
class Listener;

class ListenerClient {
    friend class Listener;

    public:
        ListenerClient(Listener *, struct tls *, const int fd, const struct sockaddr_storage);
        ~ListenerClient();

        void write(const void *data, const size_t len);

    private:
        enum class PipeEvent: uint8_t {
            // do nothing
            NoOp,
            // transmit the given packet
            SendPacket,
        };

        /// Data sent to worker thread via pipe
        struct PipeData {
            /// Type of event
            PipeEvent type;

            /// Optional message payload
            std::byte *payload = nullptr;
            /// length of payload
            size_t payloadLen = 0;

            PipeData() = default;
            PipeData(const PipeEvent _type) : type(_type) {}
        };

    private:
        void workerMain();
        void handlePipeEvent(const PipeData &);
        void handleMessage(const PacketHeader &);

    private:
        Listener *owner = nullptr;

        /// Client TLS connection
        struct tls *tls = nullptr;
        /// file descriptor for client
        int fd = -1;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;

        /// client notification pipes
        int notePipe[2] = {-1, -1};

        struct sockaddr_storage clientAddr;
};
};

#endif
