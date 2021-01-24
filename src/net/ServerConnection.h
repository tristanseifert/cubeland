#ifndef NET_SERVERCONNECTION_H
#define NET_SERVERCONNECTION_H

#include "PacketHandler.h"
#include "handlers/Auth.h"

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <util/ThreadPool.h>

struct tls;
struct tls_config;

namespace world {
struct Chunk;
class RemoteSource;
}

namespace chat {
class Manager;
}

namespace net {
struct PacketHeader;

namespace handler {
class Auth;
class PlayerInfo;
class WorldInfo;
class ChunkLoader;
class PlayerMovement;
class Time;
class BlockChange;
class Chat;
}

class ServerConnection {
    friend class world::RemoteSource;
    friend class chat::Manager;

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

        /// Sends a player position update packet
        void sendPlayerPosUpdate(const glm::vec3 &pos, const glm::vec3 &angle);

        /// Returns a list of all connected players
        std::future<std::vector<handler::Auth::Player>> getConnectedPlayers(const bool wantClientAddr = false) {
            return this->auth->getConnectedPlayers(wantClientAddr);
        }

        /// whether the connection is still actively connected or nah
        const bool isConnected() const {
            return this->connected;
        }

        /// gets a reference to the work pool to be used by client handlers
        util::ThreadPool<std::function<void(void)>> *getWorkPool() {
            return this->pool;
        }
        /// sets the reference to the work pool
        void setWorkPool(util::ThreadPool<std::function<void(void)>> *newPool) {
            this->pool = newPool;
        }

        /// set the world source that this source is using
        void setSource(world::RemoteSource *source);
        /// get world source
        world::RemoteSource *getSource() {
            return this->source;
        }

        /// Sets up a newly loaded chunk for change notifications
        void didLoadChunk(const std::shared_ptr<world::Chunk> &);
        /// Notifies server we've unloaded a chunk
        void didUnloadChunk(const std::shared_ptr<world::Chunk> &);

        /// Returns more detailed error information, if available
        std::optional<std::string> getErrorDetail() {
            return this->connectionError;
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
        handler::BlockChange *block = nullptr;
        handler::PlayerInfo *playerInfo = nullptr;
        handler::WorldInfo *worldInfo = nullptr;
        handler::ChunkLoader *chonker = nullptr;
        handler::PlayerMovement *movement = nullptr;
        handler::Time *time = nullptr;
        handler::Chat *chat = nullptr;

        /// connection flag
        bool connected = true;

        /// thread work pool
        util::ThreadPool<std::function<void(void)>> *pool = nullptr;
        /// world source  that this connection is yeeted to
        world::RemoteSource *source = nullptr;

        /// most recent connection error
        std::optional<std::string> connectionError;
};
}

#endif
