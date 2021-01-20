#ifndef SERVER_NET_LISTENERCLIENT_H
#define SERVER_NET_LISTENERCLIENT_H

#include "PacketHandler.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

        uint16_t writePacket(const uint8_t ep, const uint8_t type, const std::string &payload,
                const uint16_t tag = 0) {
            return this->writePacket(ep, type, payload.data(), payload.size(), tag);
        }
        uint16_t writePacket(const uint8_t ep, const uint8_t type,
                const std::vector<std::byte> &payload, const uint16_t tag = 0) {
            return this->writePacket(ep, type, payload.data(), payload.size(), tag);
        }

        /// builds a packet by prepending a header to the specified body
        uint16_t writePacket(const uint8_t ep, const uint8_t type, const void *data, const size_t dataLen, const uint16_t tag = 0);

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
            PipeEvent type = PipeEvent::NoOp;

            /// Optional message payload
            std::byte *payload = nullptr;
            /// length of payload
            size_t payloadLen = 0;

            PipeData() = default;
            PipeData(const PipeEvent _type) : type(_type) {}
        };

    private:
        void sendPipeData(const PipeData &);

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

        /// all packet message handlers
        std::vector<std::unique_ptr<PacketHandler>> handlers;

        /// Tag value to write in the next packet
        uint16_t nextTag = 1;
};
};

#endif
