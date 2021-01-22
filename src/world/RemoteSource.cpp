#include "RemoteSource.h"

#include "net/ServerConnection.h"
#include "net/handlers/PlayerMovement.h"

#include <io/Format.h>
#include <Logging.h>
#include <util/Thread.h>

#include <world/chunk/Chunk.h>

#include <mutils/time/profiler.h>

using namespace world;

/**
 * Sets up the world source.
 *
 * The worker thread pool is initialized.
 */
RemoteSource::RemoteSource(std::shared_ptr<net::ServerConnection> _conn, const uuids::uuid &_id,
        const size_t numThreads) : ClientWorldSource(_id), server(_conn) {
    // start worker threads
    this->pool = new util::ThreadPool<WorkItem>("RemoteSource", numThreads);
    _conn->setWorkPool(this->pool);
}

/**
 * Shuts down all of the worker threads and ensures the server connection is closed.
 */
RemoteSource::~RemoteSource() {
    // shut down workers
    delete this->pool;

    // close connection
    this->server->close();
}


/**
 * Reads a chunk from the server. We'll receive the whole chunk, at a certain point of data, and
 * then receive delta updates for any changed blocks in that chunk. Those updates continue until
 * we unsubscribe from them, which happens when the chunk is unloaded.
 */
std::future<std::shared_ptr<Chunk>> RemoteSource::getChunk(int x, int z) {
    // TODO: check cache

    // make request
    const glm::ivec2 pos(x, z); 
    return this->server->getChunk(pos);
}



/**
 * Reads a world info key.
 */
std::promise<std::vector<char>> RemoteSource::getWorldInfo(const std::string &key) {
    std::promise<std::vector<char>> prom;

    // make the request
    this->pool->queueWorkItem([&, key] {
        try {
            // try the network request
            auto future = this->server->getWorldInfo(key);
            auto value = future.get();

            // convert value, if found
            std::vector<char> temp;
            if(value) {
                temp.resize(value->size());
                memcpy(temp.data(), value->data(), value->size());
            }

            prom.set_value(temp);
        } catch(std::exception &e) {
            Logging::error("Remote: failed to get world info (key = '{}'): {}", key, e.what());
            prom.set_exception(std::current_exception());
        }
    });
    return prom;
}



/**
 * Gets a player info key.
 */
std::promise<std::vector<char>> RemoteSource::getPlayerInfo(const uuids::uuid &id, const std::string &key) {
    if(id != this->playerId) {
        throw std::runtime_error("Remote source can only get player info for current player");
    }

    // perform the network request waiting on a worker thread
    std::promise<std::vector<char>> prom;

    this->pool->queueWorkItem([&, key] {
        try {
            // try the network request
            auto future = this->server->getPlayerInfo(key);
            auto value = future.get();

            // convert value, if found
            std::vector<char> temp;
            if(value) {
                temp.resize(value->size());
                memcpy(temp.data(), value->data(), value->size());
            }

            prom.set_value(temp);
        } catch(std::exception &e) {
            Logging::error("Remote: failed to get player info (key = '{}'): {}", key, e.what());
            prom.set_exception(std::current_exception());
        }
    });

    return prom;
}
/**
 * Sets a player info key.
 */
std::future<void> RemoteSource::setPlayerInfo(const uuids::uuid &id, const std::string &key, const std::vector<char> &_value) {
    if(id != this->playerId) {
        throw std::runtime_error("Remote source can only set player info for current player");
    }

    // we need to copy the value buffer
    std::vector<std::byte> value;
    value.resize(_value.size());
    if(!_value.empty()) {
        memcpy(value.data(), _value.data(), _value.size());
    }

    return this->pool->queueWorkItem([&, key, value] {
        try {
            this->server->setPlayerInfo(key, value);
        } catch(std::exception &e) {
            Logging::error("Remote: failed to set player info (key = '{}'): {}", key, e.what());
        }
    });
}

/**
 * Waits until the write queue for blocks is empty.
 */
void RemoteSource::flushDirtyChunksSync() {
    // TODO: implement
}

/**
 * Ignore requests to force write dirty chunks, since chunks can't be marked dirty with the server
 * protocol. This is because any changes are sent on the block level.
 *
 * However, we keep this call around because it's basically a "the chunk has become unloaded" deals
 * and is used to unsubscribe from block notifications in that chunk.
 */
void RemoteSource::forceChunkWriteIfDirtySync(std::shared_ptr<Chunk> &chunk) {
    // TODO: implement
}



/**
 * Start of frame handler
 */
void RemoteSource::startOfFrame() {
    this->valid = this->server->isConnected();
}

/**
 * Sends the movement through to the server connection to be yeeted to the server. Since this is
 * called every frame, we'll do some yeetage to ensure we only actually transmit packets when the
 * values changed significantly
 */
void RemoteSource::playerMoved(const glm::vec3 &pos, const glm::vec3 &angle) {
    // check to see that the position/angles changed enough
    if(!this->forcePlayerPosSend) {
        // check position
        auto delta = pos - this->lastPos;
        if(!glm::all(glm::epsilonEqual(delta, glm::vec3(0), kPositionThreshold))) {
            goto changed;
        }

        // check angles
        delta = angle - this->lastAngle;
        if(!glm::all(glm::epsilonEqual(delta, glm::vec3(0), kAngleThreshold))) {
            goto changed;
        }

        // if we get here, neither the position nor angle updated significantly enough
        return;
    }
changed:;

    this->lastPos = pos;
    this->lastAngle = angle;
    this->forcePlayerPosSend = false;

    // send it
    this->server->sendPlayerPosUpdate(pos, angle);
}

/**
 * Returns the cached initial position, if the server sent one. Otherwise, we'll make a request
 * for the spawn position.
 *
 * Note that since we expect to have received the unsolicited initial position packet from the
 * server by the time this is called during construction of the world renderer. This shouldn't be
 * a problem since it's the first packet sent after authentication succeeds, and we'll be pre-
 * loading a few chunks anyways which takes longer.
 */
std::promise<std::pair<glm::vec3, glm::vec3>> RemoteSource::getInitialPosition() {
    auto move = this->server->movement;

    // return cached position, if available
    if(move->hasInitialPos) {
        std::promise<std::pair<glm::vec3, glm::vec3>> prom;
        prom.set_value(std::make_pair(move->position, move->angles));

        return prom;
    }

    // get spawn point
    return this->getSpawnPosition();
}

