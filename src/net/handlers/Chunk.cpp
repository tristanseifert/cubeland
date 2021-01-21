#include "Chunk.h"
#include "net/ServerConnection.h"

#include <world/chunk/Chunk.h>
#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPChunk.h>

#include <Logging.h>
#include <io/Format.h>

#include <cereal/archives/portable_binary.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

/**
 * Initializes the chunk loading packet handler.
 */
ChunkLoader::ChunkLoader(ServerConnection *_server) : PacketHandler(_server) {

}

/**
 * Notify any pending waits that we're exiting.
 */
ChunkLoader::~ChunkLoader() {
    std::lock_guard<std::mutex> lg(this->requestsLock);
    for(auto &[key, promise] : this->requests) {
        promise.set_value(nullptr);
    }
}



/**
 * We handle all world info endpoint packets.
 */
bool ChunkLoader::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointChunk) return false;
    // it musn't be more than the max value
    if(header.type >= kChunkTypeMax) return false;

    return true;
}


/**
 * Handles world info packets.
 */
void ChunkLoader::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    switch(header.type) {
        case kChunkSliceData:
            break;
        case kChunkCompletion:
            this->handleCompletion(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid chunk packet type: ${:02x}", header.type));
    }
}



/**
 * Sends a request to the server to get a particular world info key.
 */
std::future<std::shared_ptr<world::Chunk>> ChunkLoader::get(const glm::ivec2 &pos) {
    // set up the promise
    std::promise<std::shared_ptr<world::Chunk>> prom;
    auto future = prom.get_future();

    {
        std::lock_guard<std::mutex> lg(this->requestsLock);
        XASSERT(!this->requests.contains(pos), "Already waiting for chunk load for {}!", pos);
        this->requests[pos] = std::move(prom);
    }

    // allocate the bare chunk
    {
        std::lock_guard<std::mutex> lg(this->inProgressLock);

        auto chunk = std::make_shared<world::Chunk>();
        chunk->worldPos = pos;
        this->inProgress[pos] = chunk;
    }

    // build the request
    ChunkGet request;
    request.chunkPos = pos;

    Logging::trace("Sending request for chunk {}", pos);

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(request);

    // send it
    this->server->writePacket(kEndpointChunk, kChunkGet, oStream.str());
    return future;
}



/**
 * Handles a received completion callback. We'll copy out the chunk global metadata and satisfy
 * the promise for the chunk.
 */
void ChunkLoader::handleCompletion(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    ChunkCompletion comp;
    iArc(comp);

    // TODO: wait on outstanding work

    // get the chunk out of the in progress map
    std::shared_ptr<world::Chunk> chunk;
    {
        std::lock_guard<std::mutex> lg(this->inProgressLock);
        chunk = this->inProgress[comp.chunkPos];
        this->inProgress.erase(comp.chunkPos);
    }

    // TODO: verify number of slices

    // copy metadata and then satisfy the promise
    chunk->meta = comp.meta;

    std::lock_guard<std::mutex> lg(this->requestsLock);
    this->requests[comp.chunkPos].set_value(chunk);
    this->requests.erase(comp.chunkPos);
}
