#ifndef NET_SERVERCONNECTION_H
#define NET_SERVERCONNECTION_H

#include "PacketHandler.h"

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <glm/vec2.hpp>

struct tls;
struct tls_config;

namespace world {
struct Chunk;
}

namespace net {
struct PacketHeader;

namespace handler {
class Auth;
class PlayerInfo;
class WorldInfo;
class ChunkLoader;
}

class ServerConnection {
    public:
        /// Default server port
        constexpr static uint16_t kDefaultPort = 47420;

    public:
        ServerConnection(const std::string &host);
        ~ServerConnection();

        void close();
        bool authenticate();

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

        /// Reads a player info key.
        std::future<std::optional<std::vector<std::byte>>> getPlayerInfo(const std::string &key);
        /// Sets a player info key; this returns as soon as the request is sent
        void setPlayerInfo(const std::string &key, const std::vector<std::byte> &data);

        /// Reads a world info key
        std::future<std::optional<std::vector<std::byte>>> getWorldInfo(const std::string &key);

        /// Reads a chunk
        std::future<std::shared_ptr<world::Chunk>> getChunk(const glm::ivec2 &pos);

        const bool isConnected() const {
            return this->connected;
        }

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

    private:
        /// connection string
        const std::string host;

        /// socket file descriptor
        int socket = -1;
        /// client connection
        struct tls *client = nullptr;

        std::atomic_bool workerRun;
        std::unique_ptr<std::thread> worker;

        /// all packet message handlers
        std::vector<std::unique_ptr<PacketHandler>> handlers;

        /// tag value for the next packet
        uint16_t nextTag = 1;

        handler::Auth *auth = nullptr;
        handler::PlayerInfo *playerInfo = nullptr;
        handler::WorldInfo *worldInfo = nullptr;
        handler::ChunkLoader *chonker = nullptr;

        /// connection flag
        bool connected = true;
};
}

#endif
