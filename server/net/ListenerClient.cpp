#include "ListenerClient.h"

#include <Logging.h>
#include <io/Format.h>
#include <util/Thread.h>

#include <unistd.h>

#include <stdexcept>

#include <tls.h>

using namespace net;



/**
 * Creates a new listener client. Its worker thread is created.
 */
ListenerClient::ListenerClient(struct tls *_tls, const int _fd, const struct sockaddr_storage _addr) : tls(_tls), fd(_fd), clientAddr(_addr) {
    XASSERT(_tls, "Invalid TLS struct");

    // set up the worker
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&ListenerClient::workerMain, this);
}

/**
 * Closes the socket and shuts down the worker
 */
ListenerClient::~ListenerClient() {
    this->workerRun = false;

    int err = ::close(this->fd);
    if(err) {
        Logging::error("Failed to close client fd: {}", strerror(errno));
    }

    this->worker->join();
}



/**
 * Worker main loop; we try to complete the TLS handshake and then continue to try to read
 * messages from the socket, or for pending writes we want to perform.
 */
void ListenerClient::workerMain() {
    int err;

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

        }

        // cleansing
    } catch(std::exception &e) {
        Logging::error("Client {} error: {}", this->clientAddr, e.what());
    }

    // clean up
    err = tls_close(this->tls);
    if(err) {
        Logging::error("Failed to close client: {}", tls_error(this->tls));
    }

    tls_free(this->tls);

    // remove it from client
    // TODO: this
}
