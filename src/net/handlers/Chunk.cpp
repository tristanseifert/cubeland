#include "Chunk.h"
#include "net/ServerConnection.h"

#include <world/block/BlockIds.h>
#include <world/chunk/Chunk.h>
#include <world/chunk/ChunkSlice.h>
#include <world/WorldSource.h>
#include <net/PacketTypes.h>
#include <net/EPChunk.h>
#include <util/LZ4.h>

#include <Logging.h>
#include <io/Format.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

// uncomment to enable logging of chunk requests
// #define LOG_CHUNK_REQUESTS

/**
 * Initializes the chunk loading packet handler.
 */
ChunkLoader::ChunkLoader(ServerConnection *_server) : PacketHandler(_server) {
}

/**
 * Notify any pending waits that we're exiting.
 */
ChunkLoader::~ChunkLoader() {
    // max out all pending counts
    {
        std::lock_guard<std::mutex> lg(this->countsLock);

        for(auto &[pos, count] : this->counts) {
            count = world::Chunk::kMaxY;
        }

        this->countsCond.notify_all();
    }

    this->abortAll();
}

/**
 * Aborts all outstanding chunk requests.
 */
void ChunkLoader::abortAll() {
    this->acceptGets = false;

    std::lock_guard<std::mutex> lg(this->requestsLock);
    for(auto &[key, promise] : this->requests) {
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Request aborted")));
    }
    this->requests.clear();
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
    PROFILE_SCOPE(ChunkLoader);

    switch(header.type) {
        case kChunkSliceData:
            this->handleSlice(header, payload, payloadLen);
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

    if(!this->acceptGets) {
        prom.set_exception(std::make_exception_ptr(std::runtime_error("Not accepting requests")));
        return future;
    }

    {
        std::lock_guard<std::mutex> lg(this->requestsLock);
        XASSERT(!this->requests.contains(pos), "Already waiting for chunk load for {}!", pos);
        this->requests[pos] = std::move(prom);
    }

    // set up initial state
    {
        std::lock_guard<std::mutex> lg(this->inProgressLock), lg2(this->countsLock);

        auto chunk = std::make_shared<world::Chunk>();
        chunk->worldPos = pos;
        this->inProgress[pos] = chunk;

        this->counts[pos] = 0;
    }

    // build the request
    ChunkGet request;
    request.chunkPos = pos;

#if LOG_CHUNK_REQUESTS
    Logging::trace("Sending request for chunk {}", pos);
#endif

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(request);

    // send it
    this->server->writePacket(kEndpointChunk, kChunkGet, oStream.str());
    return future;
}


/**
 * Handles received slice data
 */
void ChunkLoader::handleSlice(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    ChunkSliceData data;
    iArc(data);

    // process in background
    this->server->getWorkPool()->queueWorkItem([&, data] {
        try {
            this->process(data);
        } catch(std::exception &e) {
            Logging::error("Failed to process slice {} for {}: {}", data.y, data.chunkPos, e.what());
        }
    });
}

/**
 * Worker thread callback for processing a single slice worth of data
 */
void ChunkLoader::process(const ChunkSliceData &data) {
    PROFILE_SCOPE(ProcessSlice);

    // get the chunk
    this->inProgressLock.lock();
    auto chunk = this->inProgress[data.chunkPos];
    this->inProgressLock.unlock();

    if(!chunk) {
        Logging::error("Received data for chunk {} (y = {}) but no such chunk found!", data.chunkPos, data.y);
        return;
    }

    // reverse the uuid -> uint16_t map
    std::unordered_map<uint16_t, uuids::uuid> idMap;
    for(const auto &[uuid, id] : data.typeMap) {
        idMap[id] = uuid;
    }

    // create or find a suitable ID map
    int mapId = -1;

    chunk->sliceIdMapsLock.lock();

    /*
     * Now that we have a set of all of the 16-bit block IDs used by this row, we can use this
     * to see if we've generated a map containing these 16-bit IDs in our slice struct. This
     * avoids the need to muck about with UUIDs which is relatively slow.
     */
    for(size_t i = 0; i < chunk->sliceIdMaps.size(); i++) {
        const auto &map = chunk->sliceIdMaps[i];

        for(const auto &[id, uuid] : idMap) {
            auto found = std::find(map.idMap.begin(), map.idMap.end(), uuid);
            if(found == map.idMap.end()) goto beach;
        }

        // this map contains all block IDs, yay
        mapId = i;
        // we jump down here if the map does _not_ contain one of the IDs in this row
beach:;
    }

    // failed to find a map; create a new one
    if(mapId == -1) {
        // in the first slot, we have the id for air
        std::array<uuids::uuid, 256> ids;
        ids[0] = world::kAirBlockId;

        // fill the remaining entries
        size_t i = 1;
        for(const auto &[id, uuid] : idMap) {
            ids[i] = uuid;
            ++i;
        }

        // yeet it into the chunk
        mapId = chunk->sliceIdMaps.size();

        world::ChunkRowBlockTypeMap yeet;
        yeet.idMap = ids;

        chunk->sliceIdMaps.push_back(yeet);
    }

    chunk->sliceIdMapsLock.unlock();

    // build the reverse map
    XASSERT(mapId >= 0, "Failed to select map id");
    std::unordered_map<uint16_t, uint8_t> reverseIdMap;

    const auto &map = chunk->sliceIdMaps[mapId];
    for(size_t i = 0; i < 256; i++) {
        const auto uuid = map.idMap[i];

        // air doesn't really have an entry in the typemap the server sent, so special case it
        if(uuid == world::kAirBlockId) {
            reverseIdMap[0] = i;
        } else if(!uuid.is_nil()) {
            reverseIdMap[data.typeMap.at(uuid)] = i;
        }
    }

    // decompress data
    static thread_local std::unique_ptr<util::LZ4> compressor = nullptr;
    if(!compressor) {
        compressor = std::make_unique<util::LZ4>();
    }

    std::vector<uint16_t> grid;
    grid.resize(256*256, 0);

    auto bytes = reinterpret_cast<char *>(grid.data());
    const size_t numBytes = grid.size() * sizeof(uint16_t);

    compressor->decompress(data.data.data(), data.data.size(), bytes, numBytes);

    // allocate a slice and fill it in
    auto slice = new world::ChunkSlice;

    for(size_t z = 0; z < 256; z++) {
        const size_t zOff = (z * 256);

        world::ChunkSliceRow *row = nullptr;

        // TODO: decide if we want a sparse row
        row = chunk->allocRowDense(); 
        row->typeMap = mapId;

        // fill in each column
        for(size_t x = 0; x < 256; x++) {
            row->set(x, reverseIdMap.at(grid[zOff + x]));
        }

        // store the row
        slice->rows[z] = row;
    }

    // write it into the chunk
    chunk->slices[data.y] = slice;

    // update counts
    std::lock_guard<std::mutex> lg(this->countsLock);
    this->counts[chunk->worldPos]++;
    this->countsCond.notify_all();
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

    // send to the work thread pool
    this->server->getWorkPool()->queueWorkItem([&, comp] {
        this->process(comp);
    });
}

void ChunkLoader::process(const ChunkCompletion &comp) {
    PROFILE_SCOPE(FinishChunk);

    // wait on outstanding work
    std::unique_lock<std::mutex> lk(this->countsLock);
    this->countsCond.wait(lk, [&]{
        if(!this->counts.contains(comp.chunkPos)) {
            throw std::runtime_error("Invalid chunk slices count map");
        }

        return (this->counts[comp.chunkPos] >= comp.numSlices);
    });

#if LOG_CHUNK_REQUESTS
    Logging::trace("Completed chunk {}! Total {} slices", comp.chunkPos, this->counts[comp.chunkPos]);
#endif
    this->counts.erase(comp.chunkPos);

    // get the chunk out of the in progress map
    std::shared_ptr<world::Chunk> chunk;
    {
        std::lock_guard<std::mutex> lg(this->inProgressLock);
        chunk = this->inProgress[comp.chunkPos];

        this->inProgress.erase(comp.chunkPos);
    }

    // add an observeyboi for changes
    this->server->didLoadChunk(chunk);

    // copy metadata and then satisfy the promise
    chunk->meta = comp.meta;

    std::lock_guard<std::mutex> lg(this->requestsLock);
    this->requests[comp.chunkPos].set_value(chunk);
    this->requests.erase(comp.chunkPos);
}
