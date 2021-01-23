#include "BlockChange.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include <world/WorldSource.h>
#include <world/chunk/Chunk.h>
#include <net/PacketTypes.h>
#include <net/EPBlockChange.h>

#include <io/Format.h>
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>
#include <stdexcept>

using namespace net::handler;
using namespace net::message;

std::atomic_bool BlockChange::broadcastRun;
std::unique_ptr<std::thread> BlockChange::broadcastThread;
moodycamel::BlockingConcurrentQueue<BlockChange::BroadcastItem> BlockChange::broadcastQueue;



/**
 * Remove all remaining block observers.
 */
BlockChange::~BlockChange() {
    std::lock_guard<std::mutex> lg(this->chunksLock);
}


/**
 * We handle all world info endpoint packets.
 */
bool BlockChange::canHandlePacket(const PacketHeader &header) {
    // it must be the world info endpoint
    if(header.endpoint != kEndpointBlockChange) return false;
    // it musn't be more than the max value
    if(header.type >= kBlockChangeTypeMax) return false;

    return true;
}

/**
 * Handles world info packets
 */
void BlockChange::handlePacket(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // require authentication
    if(!this->client->getClientId()) {
        throw std::runtime_error("Unauthorized");
    }

    switch(header.type) {
        case kBlockChangeUnregister:
            this->removeObserver(header, payload, payloadLen);
            break;

        case kBlockChangeReport:
            this->handleChange(header, payload, payloadLen);
            break;

        default:
            throw std::runtime_error(f("Invalid block change packet type: ${:02x}", header.type));
    }
}

/**
 * Removes an existing chunk change observer.
 */
void BlockChange::removeObserver(const PacketHeader &, const void *payload, const size_t payloadLen) {
    // deserialize the message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    BlockChangeUnregister request;
    iArc(request);

    // remove the chunk observer
    std::lock_guard<std::mutex> lg(this->chunksLock);
    if(!this->chunks.contains(request.chunkPos)) {
        Logging::error("Client {} wants unsubscribe for chunk {}, but no such registration exists",
                this->client->getClientAddr(), request.chunkPos);
        return;
    }

    this->chunks.erase(request.chunkPos);
}

/**
 * Processes a block change from the client.
 */
void BlockChange::handleChange(const PacketHeader &, const void *payload, const size_t payloadLen) {
    // deserialize the message
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    BlockChangeReport request;
    iArc(request);

    if(request.changes.empty()) {
        throw std::runtime_error("Received empty block change report");
    }

    // apply each change to the chunk
    {
        std::lock_guard<std::mutex> lg(this->chunksLock);

        for(const auto &change : request.changes) {
            auto chunk = this->chunks.at(change.chunkPos);
            chunk->setBlock(change.blockPos, change.newId, true);

            this->client->getWorld()->markChunkDirty(chunk);

            Logging::trace("Chunk {} changed block {} to {}", change.chunkPos, change.blockPos, change.newId);
        }
    }

    // build change message
    BlockChangeBroadcast broad;
    broad.changes = request.changes; // TODO: veto changes

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(broad);

    const auto str = oStream.str();

    // send to all other clients
    const auto ourId = this->client->getClientId();
    const auto thisClient = this->client;

    this->client->getListener()->forEach([&, ourId, thisClient, str](auto &client) {
        // ignore unauthenticated clients, or this client
        auto clientId = client->getClientId();
        if(!clientId) return;
        else if(client.get() == thisClient) return;
        // else if(*clientId == *ourId) return;

        // send packet
        client->writePacket(kEndpointBlockChange, kBlockChangeBroadcast, str);
    });
}



/**
 * Adds a change observer to the given chunk.
 */
void BlockChange::addObserver(const std::shared_ptr<world::Chunk> &chunk) {
    std::lock_guard<std::mutex> lg(this->chunksLock);
    this->chunks[chunk->worldPos] = chunk;
}



/**
 * Starts the broadcasting thread.
 */
void BlockChange::startBroadcaster(net::Listener *list) {
    broadcastRun = true;
    broadcastThread = std::make_unique<std::thread>(std::bind(&BlockChange::broadcasterMain, list));
}

/**
 * Terminates the broadcasting thread.
 */
void BlockChange::stopBroadcaster() {
    // push some dummy work so the thread wakes
    broadcastRun = false;
    broadcastQueue.enqueue(BroadcastItem());

    // wait for it to terminate
    broadcastThread->join();
    broadcastThread = nullptr;
}

/**
 * Main loop for the broadcast thread
 */
void BlockChange::broadcasterMain(net::Listener *listener) {
    util::Thread::setName("Block Change Broadcaster");

    BroadcastItem item;

    // try to receive work updates
    while(broadcastRun) {
        broadcastQueue.wait_dequeue(item);

        switch(item.op) {
            // no-op
            case WorkerOp::NoOp:
                break;

            // build packet for changes and broadcast it
            case WorkerOp::BlockChange:
                // TODO: should we try to coalesce reports?
                broadcasterHandleChanges(listener, item);
                break;

            // unknown opcode
            default:
                Logging::error("Unknown broadcast op: {}", item.op);
                break;
        }
    }

    // clean up
}

/**
 * Builds a block change broadcast packet for the blocks that changed in the work request, then
 * sends it to ALL clients.
 */
void BlockChange::broadcasterHandleChanges(net::Listener *listener, const BroadcastItem &item) {
    // build the broadcast
    BlockChangeBroadcast broad;
    broad.changes = item.changes;

    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(broad);

    const auto str = oStream.str();

    // send to all clients
    listener->forEach([&, str](auto &client) {
        // ignore unauthenticated clients
        auto clientId = client->getClientId();
        if(!clientId) return;

        // TODO: we could check whether the given client even cares about these updates

        // send packet
        client->writePacket(kEndpointBlockChange, kBlockChangeBroadcast, str);
    });
}
