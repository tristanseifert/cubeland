#include "Listener.h"
#include "ListenerClient.h"

#include <util/Thread.h>
#include <io/Format.h>
#include <io/ConfigManager.h>
#include <Logging.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <tls.h>

#include <chrono>
#include <cstdint>

using namespace net;



/**
 * Initializes the server listener. We load configuration, create listening socket, then spawn
 * the worker thread.
 */
Listener::Listener(world::WorldSource *_reader) : world(_reader) {
    int err;

    // set up the TLS server
    this->tls = tls_server();

    auto config = tls_config_new();

    this->buildTlsConfig(config);

    err = tls_configure(this->tls, config);
    XASSERT(err == 0, "tls_configure() failed: {}", tls_error(this->tls));

    tls_config_free(config);

    // open listening socket
    const auto portNo = io::ConfigManager::getUnsigned("listen.port", 47420);
    const auto backlog = io::ConfigManager::getUnsigned("listen.backlog", 10);

    this->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    XASSERT(this->listenFd > 0, "Failed to create socket: {}", strerror(errno));

    const int yes = 1;
    err = setsockopt(this->listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    XASSERT(!err, "Failed to set SO_REUSEADDR: {}", strerror(errno));

    struct sockaddr_in listen;
    memset(&listen, 0, sizeof(listen));

    listen.sin_family = AF_INET;
    listen.sin_addr.s_addr = INADDR_ANY;
    listen.sin_port = htons(portNo);

    err = bind(this->listenFd, (struct sockaddr *) &listen, sizeof(struct sockaddr_in));
    XASSERT(!err, "Failed to bind listening socket: {}", strerror(errno));

    err = ::listen(this->listenFd, backlog);
    XASSERT(!err, "Failed to listen on socket: {}", strerror(errno));

    // set up the chunk serializer thread pool
    const auto serializerThreads = io::ConfigManager::getUnsigned("world.chunkSerializerThreads", 4);
    Logging::debug("Chunk serializer threads: {}", serializerThreads);
    this->serializerPool = new util::ThreadPool<WorkItem>("Chunk Serializer", serializerThreads);

    // create the work thread
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&Listener::workerMain, this);

    this->murderThread = std::make_unique<std::thread>(&Listener::murdererMain, this);
    this->saverThread = std::make_unique<std::thread>(&Listener::saverMain, this);
}

/**
 * Fills in a server TLS configuration.
 */
void Listener::buildTlsConfig(struct tls_config *cfg) {
    int err;

    // protocols (by default, only TLS 1.2+)
    uint32_t protocols = 0;
    const auto protocolStr = io::ConfigManager::get("tls.protocols", "secure");

    err = tls_config_parse_protocols(&protocols, protocolStr.c_str());
    XASSERT(err == 0, "tls_config_parse_protocols() failed: {}", tls_config_error(cfg));

    err = tls_config_set_protocols(cfg, protocols);
    XASSERT(err == 0, "tls_config_set_protocols() failed: {}", tls_config_error(cfg));

    // cubeland protocol
    err = tls_config_set_alpn(cfg, "cubeland/1.0");
    XASSERT(err == 0, "tls_config_set_alpn() failed: {}", tls_config_error(cfg));

    // load ciphers (using secure defaults otherwise)
    const auto cipherStr = io::ConfigManager::get("tls.ciphers", "secure");

    err = tls_config_set_ciphers(cfg, cipherStr.c_str());
    XASSERT(err == 0, "tls_config_set_ciphers() failed: {}", tls_config_error(cfg));

    // enable ephemeral Diffie-Hellman keys; this allows forward secrecy
    err = tls_config_set_dheparams(cfg, "auto");
    XASSERT(err == 0, "tls_config_set_dheparams() failed: {}", tls_config_error(cfg));

    // certificate and key
    const auto certPath = io::ConfigManager::get("tls.cert", "");
    err = tls_config_set_cert_file(cfg, certPath.c_str());
    XASSERT(err == 0, "Couldn't load cert: {}", tls_config_error(cfg));

    const auto keyPath = io::ConfigManager::get("tls.key", "");
    err = tls_config_set_key_file(cfg, keyPath.c_str());
    XASSERT(err == 0, "Couldn't load key: {}", tls_config_error(cfg));
}

/**
 * Ensures we accept no new requests, and notifies all connected clients that we're quitting.
 */
Listener::~Listener() {
    // stop accepting new requests
    this->workerRun = false;

    this->saverThread->join();

    this->removeClient(nullptr);

    // exit thread pools
    delete this->serializerPool;

    // signal clients we're quitting
    {
        std::lock_guard<std::mutex> lg(this->clientLock);
        Logging::debug("Closing {} remaining clients", this->clients.size());

        this->clients.clear();
    }

    // release SSL resources and listening socket
    tls_close(this->tls);
    tls_free(this->tls);

    ::close(this->listenFd);

    // finally, shut down the work thread
    this->worker->join();
    this->murderThread->join();
}



/**
 * Run loop for the server
 */
void Listener::workerMain() {
    int fd, err;
    struct sockaddr_storage addr;
    socklen_t addrLen;

    // set up
    util::Thread::setName("Listener");

    while(this->workerRun) {
        // accept connection
        addrLen = sizeof(addr);
        fd = accept(this->listenFd, (struct sockaddr *) &addr, &addrLen);
        if(fd == -1) {
            // connection abort caused if we closed the socket
            if(errno == ECONNABORTED) {
                continue;
            }

            // otherwise, log the error
            Logging::warn("Failed to accept client connection: {}", strerror(errno));
            continue;
        }

        // get TLS connection from it
        struct tls *tlsClient = nullptr;
        err = tls_accept_socket(this->tls, &tlsClient, fd);
        if(err) {
            Logging::error("Failed to accept TLS client {}: {}", addr, tls_error(this->tls));

            ::close(fd);
            continue;
        }

        // create client
        auto client = std::make_unique<ListenerClient>(this, tlsClient, fd, addr);
        {
            std::lock_guard<std::mutex> lg(this->clientLock);
            this->clients.push_back(std::move(client));
        }
    }
}



/**
 * Main loop for the client garbage collection thread
 */
void Listener::murdererMain() {
    util::Thread::setName("Client Deleter");

    // dequeue pointers to erase
    ListenerClient *clientPtr = nullptr;

    while(this->workerRun) {
        this->clientsToMurder.wait_dequeue(clientPtr);
        if(!clientPtr) continue;

        std::lock_guard<std::mutex> lg(this->clientLock);
        this->clients.erase(std::remove_if(this->clients.begin(), this->clients.end(), 
        [clientPtr](auto &client) {
            return (client.get() == clientPtr);
        }), this->clients.end());
    }
}

/**
 * Main loop for the saving thread
 *
 * We sleep for a fixed amount on each loop iteration, and invoke the save method of all clients.
 */
void Listener::saverMain() {
    util::Thread::setName("Client Saver");

    while(this->workerRun) {
        // invoke clients save methods
        try {
            std::lock_guard<std::mutex> lg(this->clientLock);
            for(auto &client : this->clients) {
                client->save();
            }
        } catch(std::exception &e) {
            Logging::error("Failed to save client data: {}", e.what());
        }

        // yeet
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
