#include "ListenerClient.h"
#include "Listener.h"

#include "handlers/Auth.h"
#include "handlers/Chunk.h"
#include "handlers/WorldInfo.h"
#include "handlers/PlayerInfo.h"

#include <Logging.h>
#include <io/Format.h>
#include <util/Math.h>
#include <util/Thread.h>
#include <net/PacketTypes.h>

#include <unistd.h>
#include <poll.h>

#include <cstring>
#include <stdexcept>

#include <tls.h>

using namespace net;

// uncomment to log all received packet contents
// #define LOG_PACKETS


/**
 * Creates a new listener client. Its worker thread is created.
 */
ListenerClient::ListenerClient(Listener *_list, struct tls *_tls, const int _fd, 
        const struct sockaddr_storage _addr) : owner(_list), tls(_tls), fd(_fd), clientAddr(_addr) {
    XASSERT(_tls, "Invalid TLS struct");

    int err;

    // disable sigpipe on the socket
    const int optval = 1;
    err = setsockopt(_fd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(int));
    XASSERT(!err, "Failed to set SO_NOSIGPIPE: {}", strerror(errno));

    // set up notification pipe. the read end is non-blocking
    err = pipe(this->notePipe);
    XASSERT(!err, "Failed to create notification pipe: {}", strerror(errno));


    err = fcntl(this->notePipe[0], F_GETFL);
    XASSERT(err != -1, "Failed to get read pipe flags: {}", strerror(errno));
    err = fcntl(this->notePipe[0], F_SETFL, err | O_NONBLOCK);
    XASSERT(err != -1, "Failed to set read pipe flags: {}", strerror(errno));

    // initialize packet handlers
    this->auth = new handler::Auth(this);
    this->handlers.emplace_back(new handler::ChunkLoader(this));
    this->handlers.emplace_back(new handler::PlayerInfo(this));
    this->handlers.emplace_back(new handler::WorldInfo(this));
    this->handlers.emplace_back(this->auth);

    // set up the worker
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&ListenerClient::workerMain, this);
}

/**
 * Closes the socket and shuts down the worker
 */
ListenerClient::~ListenerClient() {
    int err;

    // request the worker thread shuts down
    this->workerRun = false;

    PipeData d(PipeEvent::NoOp);
    err = ::write(this->notePipe[1], &d, sizeof(d));
    if(err == -1) {
        // ignore if the pipe's already been closed
        if(errno != EPIPE) {
            Logging::error("Failed to write shut down request to pipe: {}", strerror(errno));
        }
    }

    this->worker->join();

    // close socket, wait for it to terminate, then close pipes
    err = ::close(this->fd);
    if(err) {
        Logging::error("Failed to close client fd: {}", strerror(errno));
    }

    ::close(this->notePipe[1]);
}



/**
 * Sends a event through the yenpipe.
 */
void ListenerClient::sendPipeData(const PipeData &d) {
    int err = ::write(this->notePipe[1], &d, sizeof(d));
    if(err == -1) {
        Logging::error("Failed to write request to pipe: {}", strerror(errno));
    }
}


/**
 * Builds a valid packet header for the packet, then sends it. The payload is copied.
 *
 * @return Tag of the packet. You may specify the tag manually, or generate one automagically
 */
uint16_t ListenerClient::writePacket(const uint8_t ep, const uint8_t type, const void *data,
        const size_t dataLen, const uint16_t _tag) {
    // get the real tag to apply to the packet
    auto tag = _tag;
    if(!tag) {
again:;
        tag = this->nextTag++;
        if(!tag) goto again;
    }

    // allocate memory
    size_t reqPacketSize = sizeof(PacketHeader) + dataLen;
    if(reqPacketSize & 0x3) {
        reqPacketSize += 4 - (reqPacketSize & 0x3);
    }

    auto buf = new std::byte[reqPacketSize];
    memset(buf, 0, reqPacketSize);

    // construct header
    auto hdr = reinterpret_cast<PacketHeader *>(buf);
    hdr->endpoint = ep;
    hdr->type = type;
    hdr->length = IntCeil(dataLen, 4);

    memcpy(hdr->payload, data, dataLen);

    // send it
    hdr->tag = htons(tag);
    hdr->length = htons(hdr->length);

    PipeData pd(PipeEvent::SendPacket);
    pd.payload = buf;
    pd.payloadLen = reqPacketSize;
    this->sendPipeData(pd);

    // clean up
    return tag;
}



/**
 * Worker main loop; we try to complete the TLS handshake and then continue to try to read
 * messages from the socket, or for pending writes we want to perform.
 */
void ListenerClient::workerMain() {
    int err;
    struct pollfd pfd[2];
    PacketHeader hdr;
    bool yenpipePending = false;

    util::Thread::setName(f("Client Worker {}", this->clientAddr));

    try {
        // complete handshake
shakeAgain:;
        err = tls_handshake(this->tls);
        if(err) {
            if(err == TLS_WANT_POLLIN || err == TLS_WANT_POLLOUT) goto shakeAgain;
            throw std::runtime_error(f("Failed to complete handshake: {}", tls_error(this->tls)));
        }

        // configure socket in non-blocking mode

        // read messages
        while(this->workerRun) {
            // block on the note pipe (read end)
            pfd[0].fd = this->notePipe[0];
            pfd[0].events = POLLIN;

            // and the raw socket the client is connected to
            pfd[1].fd = this->fd;
            pfd[1].events = POLLIN;

            // block on the client socket and notification pipe
            err = poll(pfd, 2, -1);

            if(err == 0) continue; // timeout expired
            else if(err == -1) {
                throw std::runtime_error(f("poll() failed: {}", strerror(errno)));
            }

            // messages in note pipe?
            if(pfd[0].revents & POLLIN || yenpipePending) {
                PipeData d;

                do {
                    err = read(this->notePipe[0], &d, sizeof(d));
                    if(err == -1) {
                        // try this again later
                        if(errno == EAGAIN) {
                            yenpipePending = true;
                            continue;
                        }

                        throw std::runtime_error(f("couldn't read yenpipe: {}", strerror(errno)));
                    } else if(!err) {
                        // nothing left in the pipe
                        yenpipePending = false;
                        continue;
                    }

                    // process the event
                    this->handlePipeEvent(d);
                    yenpipePending = false;
                } while(err > 0);
            }

            // try to read packet header
            if(pfd[1].revents & POLLIN) {
readAgain:;
                err = tls_read(this->tls, &hdr, sizeof(hdr));
                if(err == TLS_WANT_POLLIN) {
                    goto readAgain;
                } else if(err == TLS_WANT_POLLOUT) {
                    goto readAgain;
                } else if(err == -1) {
                    throw std::runtime_error(f("tls_read() failed: {}", tls_error(this->tls)));
                } else if(err == 0) {
                    goto beach;
                }

                if(err != sizeof(hdr)) {
                    Logging::error("Partial header read from {}: {} bytes, expected {}",
                            this->clientAddr, err, sizeof(hdr));
                    continue;
                }

                // handle the message
                hdr.length = ntohs(hdr.length);
                hdr.tag = ntohs(hdr.tag);
                this->handleMessage(hdr);
            }
        }

beach:;
    } catch(std::exception &e) {
        Logging::error("Client {} error: {}", this->clientAddr, e.what());
    }

    // close connection
    this->connected = false;
    Logging::debug("Cleaning up client {}", this->clientAddr);

closeAgain:;
    err = tls_close(this->tls);
    if(err) {
        if(err == TLS_WANT_POLLIN || err == TLS_WANT_POLLOUT) goto closeAgain;
        Logging::error("Failed to close client {}: {}", this->clientAddr, tls_error(this->tls));
    }

    // release resources and close the read end of the yenpipe
    tls_free(this->tls);
    ::close(this->notePipe[0]);

    // remove it from client
    this->owner->removeClient(this);
}

/**
 * Handles an event received on the yenpipe.
 */
void ListenerClient::handlePipeEvent(const PipeData &data) {
    switch(data.type) {
        case PipeEvent::SendPacket: {
            size_t len = data.payloadLen;
            auto buf = data.payload;

            while(len > 0) {
                int err;
                err = tls_write(this->tls, buf, len);

                if(err == TLS_WANT_POLLIN || err == TLS_WANT_POLLOUT) {
                    continue;
                } else if(err == -1) {
                    throw std::runtime_error(f("tls_write() failed: {}", tls_error(this->tls)));
                }

                buf += err;
                len -= err;
            }

            // clean up the payload
            delete[] data.payload;
            break;
        }
        case PipeEvent::NoOp:
            break;
        default:
            throw std::runtime_error(f("Invalid pipe event type: {}", data.type));
    }
}

/**
 * Handle a received message.
 */
void ListenerClient::handleMessage(const PacketHeader &header) {
    int err;

    // read the remainder of the packet
    std::vector<std::byte> buffer;

    if(header.length) {
        buffer.resize(header.length * 4);

        auto writePtr = buffer.data();
        size_t toRead = buffer.size();

        while(toRead > 0) {
            err = tls_read(this->tls, writePtr, toRead);
            if(err == TLS_WANT_POLLIN || err == TLS_WANT_POLLOUT) {
                continue;
            }
            else if(err == -1) {
                throw std::runtime_error(f("tls_read() failed: {}", tls_error(this->tls)));
            } else if(err == 0) {
                throw std::runtime_error("Connection closed");
            }

            writePtr += err;
            toRead -= err;
        }
    }

#if LOG_PACKETS
    Logging::trace("Received packet {:02x}:{:02x} length {}: payload {}", header.endpoint,
            header.type, header.length, hexdump(buffer.begin(), buffer.end()));
#endif

    // invoke the appropriate handler
    for(auto &handler : this->handlers) {
        if(handler->canHandlePacket(header)) {
            handler->handlePacket(header, buffer.data(), buffer.size());
            return;
        }
    }

    Logging::warn("Unhandled packet ({}) {:02x}:{:02x} length {}: payload {}", this->clientAddr,
            header.endpoint, header.type, header.length, hexdump(buffer.begin(), buffer.end()));
}

/**
 * Return client auth id
 */
std::optional<uuids::uuid> ListenerClient::getClientId() const {
    return this->auth->getClientId();
}

/**
 * Return world pointer
 */
world::WorldSource *ListenerClient::getWorld() const {
    return this->owner->getWorld();
}
