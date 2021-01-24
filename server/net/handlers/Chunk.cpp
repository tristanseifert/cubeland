#include "Chunk.h"
#include "BlockChange.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <world/block/BlockIds.h>
#include <world/chunk/Chunk.h>
#include <world/chunk/ChunkSlice.h>
#include <net/PacketTypes.h>
#include <net/EPChunk.h>
#include <util/LZ4.h>
#include <util/ThreadPool.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>

// uncomment to enable logging of the received and sent packets
// #define LOG_PACKETS
// uncomment to enable logging of chunk loading
// #define LOG_LOAD

using namespace net::handler;
using namespace net::message;

std::mutex ChunkLoader::cacheLock;
std::unordered_map<glm::ivec2, std::weak_ptr<world::Chunk>> ChunkLoader::cache;

/**
 * Waits for all pending completions.
 */
ChunkLoader::~ChunkLoader() {
    std::lock_guard<std::mutex> lg(this->completionsLock);
    for(auto &[pos, future] : this->completions) {
        future.get();
    }
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

#ifdef LOG_PACKETS
    Logging::trace("Request chunk: {}", request.chunkPos);
#endif

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
        auto fut = pool->queueWorkItem([&, chunk] {
            if(!this->client->isConnected()) return;
            this->sendSlices(chunk);
        });

        std::lock_guard<std::mutex> lg(this->completionsLock);
        this->completions[chunk->worldPos] = std::move(fut);
    }
    // otherwise, read it from the world source
    else {
        auto pos = request.chunkPos;

        auto fut = pool->queueWorkItem([&, pos, world] {
            // bail if client is disconnected
            if(!this->client->isConnected()) return;

            // get chunk and store in cache
            auto future = world->getChunk(pos.x, pos.y);
            auto chunk = future.get();

#ifdef LOG_LOAD
            Logging::trace("Loaded chunk: {}", (void *) chunk.get());
#endif

            {
                std::lock_guard<std::mutex> lg(cacheLock);
                cache[pos] = chunk;
            }

            // then process as normal
            if(!this->client->isConnected()) return;
            this->sendSlices(chunk);
        });

        std::lock_guard<std::mutex> lg(this->completionsLock);
        this->completions[pos] = std::move(fut);
    }
}

/**
 * Worker callback invoked to send data slices for a particular chunk.
 */
void ChunkLoader::sendSlices(const std::shared_ptr<world::Chunk> &chunk) {
    if(!this->client || !this->client->getListener() || 
            !this->client->getListener()->getSerializerPool()) {
        return;
    }

    auto pool = this->client->getListener()->getSerializerPool();
    std::vector<std::future<void>> sendSliceFutures;

    size_t numSlices = 0;

    // build the ID maps
    uint16_t nextType = 1;
    Maps maps;

    for(const auto &map : chunk->sliceIdMaps) {
        std::unordered_map<uint8_t, uint16_t> temp;

        for(size_t i = 0; i < map.idMap.size(); i++) {
            const auto &id = map.idMap[i];

            // ignore empty slots
            if(id.is_nil()) continue;
            // air is always index 0
            else if(id == world::kAirBlockId) {
                temp[i] = 0;
            }
            // otherwise, get the type ID or allocate anew
            else {
                // allocate ID if needed
                if(!maps.gridUuidMap.contains(id)) {
                    maps.gridUuidMap[id] = nextType++;
                }

                temp[i] = maps.gridUuidMap.at(id);
            }
        }

        // store the completed map
        maps.rowToGrid.push_back(temp);
    }

    // process each slice with data
    for(size_t y = 0; y < world::Chunk::kMaxY; y++) {
        auto slice = chunk->slices[y];
        if(!slice) continue;

        sendSliceFutures.emplace_back(pool->queueWorkItem([&, maps, y, chunk, slice] {
            if(!this->client->isConnected()) return;
            this->sendSlice(chunk, maps, slice, y);
        }));

        numSlices++;
    }

    // wait for all slices to finish processing
    for(auto &future : sendSliceFutures) {
        future.get();
    }

    // send the chunk completion message
    if(!this->client || !this->client->isConnected()) return;
    this->sendCompletion(chunk, numSlices);

    // remove future
    std::lock_guard<std::mutex> lg(this->completionsLock);
    this->completions.erase(chunk->worldPos);
}

/**
 * Serializes all blocks in the given slice and sends it to the client.
 */
void ChunkLoader::sendSlice(const std::shared_ptr<world::Chunk> &chunk, const Maps &maps, const world::ChunkSlice *slice, const size_t y) {
    // set up the output bufer and map
    std::vector<uint16_t> outBuf;
    outBuf.resize(256 * 256, 0);

    // iterate over all rows
    for(size_t z = 0; z < 256; z++) {
        auto row = slice->rows[z];
        if(!row) continue;

        const size_t zOff = (z * 256);

        // iterate over all columns in the row
        // const auto &map = chunk->sliceIdMaps[row->typeMap];
        const auto &map = maps.rowToGrid[row->typeMap];

        for(size_t x = 0; x < 256; x++) {
            const auto rawValue = row->at(x);
            outBuf[zOff + x] = map.at(rawValue);
        }
    }

    // TODO: extract block metas

    // set up the per-thread compressor
    static thread_local std::unique_ptr<util::LZ4> compressor = nullptr;
    if(!compressor) {
        compressor = std::make_unique<util::LZ4>();
    }

    // compress the map
    std::vector<char> compressed;

    const char *bytes = reinterpret_cast<const char *>(outBuf.data());
    const size_t numBytes = outBuf.size() * sizeof(uint16_t);

    compressor->compress(bytes, numBytes, compressed);

    // build the output
    ChunkSliceData out;
    out.chunkPos = chunk->worldPos;
    out.y = y;
    out.typeMap = maps.gridUuidMap;

    out.data.resize(compressed.size());
    memcpy(out.data.data(), compressed.data(), compressed.size());

    // serialize and send it
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(out);

    this->client->writePacket(kEndpointChunk, kChunkSliceData, oStream.str());
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

#if LOG_PACKETS
    Logging::trace("Sent completion for {}: {} slices", chunk->worldPos, numSlices);
#endif
    this->client->writePacket(kEndpointChunk, kChunkCompletion, oStream.str());

    // register for chunk change notifications (via block change request handler)
    this->client->addChunkObserver(chunk);

    // remove from the pending queue
    std::lock_guard<std::mutex> lg(this->dupesLock);
    this->dupes.erase(chunk->worldPos);
}

