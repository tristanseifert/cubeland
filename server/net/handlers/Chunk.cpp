#include "Chunk.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <world/chunk/Chunk.h>
#include <world/chunk/ChunkSlice.h>
#include <net/PacketTypes.h>
#include <net/EPChunk.h>
#include <util/ThreadPool.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

std::mutex ChunkLoader::cacheLock;
std::unordered_map<glm::ivec2, std::weak_ptr<world::Chunk>> ChunkLoader::cache;

/**
 * Initializes the chunk handler
 */
ChunkLoader::ChunkLoader(ListenerClient *_client) : PacketHandler(_client) {
}

/**
 * We handle all chunk endpoint packets.
 */
bool ChunkLoader::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointChunk) return false;
    // it musn't be more than the max value
    if(header.type >= kChunkTypeMax) return false;

    return true;
}

/**
 * Handles chunk data request packets 
 */
void ChunkLoader::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        case kChunkGet:
            this->handleGet(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid chunk packet type: ${:02x}", header.type));
    }
}

/**
 * Handles request for getting a chunk.
 *
 * We'll first check to see whether the chunk is cached.
 */
void ChunkLoader::handleGet(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    auto world = this->client->getWorld();
    auto pool = this->client->getListener()->getSerializerPool();

    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    ChunkGet request;
    iArc(request);

    Logging::trace("Request chunk: {}", request.chunkPos);

    // ignore if duplicate request
    {
        std::lock_guard<std::mutex> lg(this->dupesLock);
        if(this->dupes.contains(request.chunkPos)) {
            Logging::warn("Client {} sent duplicate chunk request for {}!",
                    this->client->getClientAddr(), request.chunkPos);
            return;
        }

        this->dupes.insert(request.chunkPos);
    }

    // check cache for the chunk
    std::shared_ptr<world::Chunk> chunk = nullptr;

    {
        std::lock_guard<std::mutex> lg(cacheLock);
        if(cache.contains(request.chunkPos)) {
            chunk = cache[request.chunkPos].lock();

            // remove ptr from cache if the chunk has been deallocated
            if(!chunk) {
                cache.erase(request.chunkPos);
            }
        }
    }

    // we've found the chunk in cache; send the slices in the background
    if(chunk) {
        pool->queueWorkItem([&, chunk] {
            this->sendSlices(chunk);
        });
    }
    // otherwise, read it from the world source
    else {
        auto pos = request.chunkPos;

        pool->queueWorkItem([&, pos, world] {
            // get chunk and store in cache
            auto future = world->getChunk(pos.x, pos.y);
            auto chunk = future.get();

            Logging::trace("Loaded chunk: {}", (void *) chunk.get());

            {
                std::lock_guard<std::mutex> lg(cacheLock);
                cache[pos] = chunk;
            }

            // then process as normal
            this->sendSlices(chunk);
        });
    }
}

/**
 * Worker callback invoked to send data slices for a particular chunk.
 */
void ChunkLoader::sendSlices(const std::shared_ptr<world::Chunk> &chunk) {
    auto pool = this->client->getListener()->getSerializerPool();
    std::vector<std::future<void>> sendSliceFutures;

    size_t numSlices = 0;

    // process each slice with data
    for(size_t y = 0; y < world::Chunk::kMaxY; y++) {
        auto slice = chunk->slices[y];
        if(!slice) continue;

        sendSliceFutures.emplace_back(pool->queueWorkItem([&, chunk, slice] {
            this->sendSlice(chunk, slice);
        }));

        numSlices++;
    }

    // wait for all slices to finish processing
    for(auto &future : sendSliceFutures) {
        future.get();
    }

    // send the chunk completion message
    this->sendCompletion(chunk, numSlices);
}

/**
 * Serializes all blocks in the given slice and sends it to the client.
 */
void ChunkLoader::sendSlice(const std::shared_ptr<world::Chunk> &chunk, const world::ChunkSlice *slice) {
    // TODO: implement
}



/**
 * After all slices have been sent, submit a completion message.
 */
void ChunkLoader::sendCompletion(const std::shared_ptr<world::Chunk> &chunk, const size_t numSlices) {
    // build the message
    ChunkCompletion comp;
    comp.numSlices = numSlices;
    comp.meta = chunk->meta;
    comp.chunkPos = chunk->worldPos;

    // send it
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(comp);

    Logging::trace("Sent completion for {}: {} slices", chunk->worldPos, numSlices);
    this->client->writePacket(kEndpointChunk, kChunkCompletion, oStream.str());

    // TODO: register for chunk change notifications (via block change request handler)

    // remove from the pending queue
    std::lock_guard<std::mutex> lg(this->dupesLock);
    this->dupes.erase(chunk->worldPos);
}

