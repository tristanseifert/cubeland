#include "BlockChange.h"
#include "net/ServerConnection.h"

#include <world/block/BlockIds.h>
#include <world/chunk/Chunk.h>
#include <net/PacketTypes.h>
#include <net/EPBlockChange.h>

#include <Logging.h>
#include <io/Format.h>

#include <mutils/time/profiler.h>
#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;



/**
 * Checks if we can handle the given packet.
 */
bool BlockChange::canHandlePacket(const PacketHeader &header) {
    // it must be the correct endpoint
    if(header.endpoint != kEndpointBlockChange) return false;
    // it musn't be more than the max value
    if(header.type >= kBlockChangeTypeMax) return false;

    return true;
}


/**
 * Handles world info packets.
 */
void BlockChange::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    PROFILE_SCOPE(BlockChange);

    switch(header.type) {
        // blocks have changed
        case kBlockChangeBroadcast:
            this->updateChunks(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid block change packet type: ${:02x}", header.type));
    }
}

/**
 * Processes received block changes.
 *
 * Note that this _can_ cause lockups and other shitty behavior if we get echoed back block changes
 * for what we did, since we'll go and run the callbacks again. So, since callbacks are run
 * synchronously, we just set a flag to inhibit the generation of change notifications back to
 * the server.
 */
void BlockChange::updateChunks(const PacketHeader &, const void *payload, const size_t payloadLen) {
    // deserialize the message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    BlockChangeBroadcast broad;
    iArc(broad);

    // apply the changes
    this->inhibitChangeReports = true;

    std::lock_guard<std::mutex> lg(this->chunksLock);
    for(const auto &change : broad.changes) {
        auto &chunk = this->chunks.at(change.chunkPos);

        chunk->setBlock(change.blockPos, change.newId, true);
    }

    this->inhibitChangeReports = false;
}


/**
 * Chunk change callback; we'll generate a change report and yeet it on to the server.
 */
void BlockChange::chunkChanged(world::Chunk *chunk, const glm::ivec3 &relBlock,
        const world::Chunk::ChangeHints hints, const uuids::uuid &id) {
    if(this->inhibitChangeReports) return;

    // prepare the report
    BlockChangeInfo info;

    info.chunkPos = chunk->worldPos;
    info.blockPos = relBlock;

    // set the ID of the block that was set
    if(hints & world::Chunk::ChangeHints::kBlockRemoved) {
        info.newId = world::kAirBlockId;
    } else {
        info.newId = id;
    }

    // build the rest of the message and send it
    BlockChangeReport report;
    report.changes.push_back(info);

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(report);

    this->server->writePacket(kEndpointBlockChange, kBlockChangeReport, oStream.str());
}



/**
 * Callback for chunks that've loaded and need to get change notifications.
 *
 * This is currently a no-op; the server will automatically register us for chunk change
 * notifications when we register. We'll simply add the change observer to the chunk.
 */
void BlockChange::startChunkNotifications(const std::shared_ptr<world::Chunk> &chunk) {
    using namespace std::placeholders;

    std::lock_guard<std::mutex> lg(this->observersLock), lg2(this->chunksLock);

    auto token = chunk->registerChangeCallback(std::bind(&BlockChange::chunkChanged, this, _1, _2, _3, _4));
    this->observers[chunk->worldPos] = token;
    this->chunks[chunk->worldPos] = chunk;
}

/**
 * Sends a message to the server to stop receiving changes for the given chunk. We'll also remove
 * our chunk change callback.
 */
void BlockChange::stopChunkNotifications(const std::shared_ptr<world::Chunk> &chunk) {
    // remove our observer
    {
        std::lock_guard<std::mutex> lg(this->observersLock), lg2(this->chunksLock);

        if(this->observers.contains(chunk->worldPos)) {
            const auto token = this->observers.at(chunk->worldPos);
            chunk->unregisterChangeCallback(token);

            this->observers.erase(chunk->worldPos);
        }

        this->chunks.erase(chunk->worldPos);
    }

    // tell server to get bent
    BlockChangeUnregister unsub;
    unsub.chunkPos = chunk->worldPos;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(unsub);

    this->server->writePacket(kEndpointBlockChange, kBlockChangeUnregister, oStream.str());
}

