#ifndef NET_SERVERCONNECTION_H
#define NET_SERVERCONNECTION_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>

struct tls;
struct tls_config;

namespace net {
struct PacketHeader;

class ServerConnection {
    public:
        /// Default server port
        constexpr static uint16_t kDefaultPort = 47420;

    public:
        ServerConnection(const std::string &host);
        ~ServerConnection();

        void authenticate();

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

        /// yenpipes for notifying worker thread
        int notePipe[2] = {-1, -1};

    private:
        void buildTlsConfig(struct tls_config *);
        void connect(const std::string &connectTo, std::string &servname);

        void workerMain();
        void workerHandleEvent(const PipeData &);
        void workerHandleMessage(const PacketHeader &);

        void sendPipeData(const PipeData &);

        uint16_t preparePacket(PacketHeader *);

    private:
        /// connection string
        const std::string host;

        /// socket file descriptor
        int socket = -1;
        /// client connection
        struct tls *client = nullptr;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;

        /// tag value for the next packet
        uint16_t nextTag = 1;
};
}

#endif
