#include "ServerConnection.h"

#include <Logging.h>
#include <io/Format.h>
#include <io/PathHelper.h>
#include <util/Thread.h>
#include <net/PacketTypes.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>

#include <tls.h>

#include <regex>

using namespace net;

/**
 * Regex for getting the port number out of a host:port string
 */
const static std::regex kPortRegex("^([^:]+)(?::)(\\d+)");



/**
 * Creates a new server connection to the specified server.
 *
 * @param host Address or DNS name of the server. The port may be specified as in "host:port" if
 * not using the default.
 */
ServerConnection::ServerConnection(const std::string &host) {
    int err;

    // resolve hostname and connect a socket
    std::string servname;
    this->connect(host, servname);

    // configure a TLS connection and connect it on our socket
    this->client = tls_client();
    XASSERT(this->client, "Failed to create TLS client");

    auto config = tls_config_new();
    XASSERT(config, "Failed to allocate TLS config");

    this->buildTlsConfig(config);

    err = tls_configure(this->client, config);
    XASSERT(err == 0, "tls_configure() failed: {}", tls_error(this->client));

    tls_config_free(config);

    err = tls_connect_socket(this->client, this->socket, servname.c_str());
    if(err) {
        throw std::runtime_error(f("TLS connection failed: {}", tls_error(this->client)));
    }

    // complete TLS handshake
    shakeAgain:;
    err = tls_handshake(this->client);
    if(err) {
        if(err == TLS_WANT_POLLIN || err == TLS_WANT_POLLOUT) goto shakeAgain;
        throw std::runtime_error(f("TLS handshake failed: {}", tls_error(this->client)));
    }

    // set up notification pipe. the read end is non-blocking
    err = pipe(this->notePipe);
    XASSERT(!err, "Failed to create notification pipe: {}", strerror(errno));

    err = fcntl(this->notePipe[0], F_GETFL);
    XASSERT(err != -1, "Failed to get read pipe flags: {}", strerror(errno));
    err = fcntl(this->notePipe[0], F_SETFL, err | O_NONBLOCK);
    XASSERT(err != -1, "Failed to set read pipe flags: {}", strerror(errno));

    // start our worker thread
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&ServerConnection::workerMain, this);
}

/**
 * Establishes a TCP socket to the given hostname, and extract the server name to use for TLS cert
 * verification.
 *
 * The hostname may be an IP address or a DNS name. In both cases, it may be a plain name or be
 * in the format of "host:port" to use a non-standard port.
 */
void ServerConnection::connect(const std::string &host, std::string &servname) {
    struct addrinfo hints, *res, *res0;
    int err, sock = -1;
    std::string resolve, portStr;

    // figure out the hostname and port to use
    if(host.find(':') == std::string::npos) {
        resolve = host;
        servname = host;
        portStr = std::to_string(kDefaultPort);
    } else {
        std::smatch m;

        if(!std::regex_search(host, m, kPortRegex)) {
            throw std::runtime_error("Failed to extract port number");
        }

        resolve = m[1].str();
        portStr = m[2].str();
    }

    Logging::trace("Host {}, port {}", resolve, portStr);

    // get the server's connection address
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    // try first as a numeric IPv4 address
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_NUMERICHOST;

    if(getaddrinfo(resolve.c_str(), portStr.c_str(), &hints, &res0) != 0) {
        // try again as an IPv6 literal
        hints.ai_family = AF_INET6;
        if(getaddrinfo(resolve.c_str(), portStr.c_str(), &hints, &res0) != 0) {
            // now attempt to do a DNS resolvification
            hints.ai_family = AF_UNSPEC;
            hints.ai_flags = AI_ADDRCONFIG;

            if((err = getaddrinfo(resolve.c_str(), portStr.c_str(), &hints, &res0)) != 0) {
                throw std::runtime_error(f("Failed to resolve hostname: {}", gai_strerror(err)));
            }
        }
    }

    // resolved; so create a socket and connect
    for(res = res0; res; res = res->ai_next) {
        sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if(sock == -1) {
            Logging::warn("Failed to open socket to {}: {}",
                    *((struct sockaddr_storage *) res->ai_addr), strerror(errno));
            continue;
        }

        err = ::connect(sock, res->ai_addr, res->ai_addrlen);
        if(err == -1) {
            Logging::warn("Failed to connect to {}: {}",
                    *((struct sockaddr_storage *) res->ai_addr), strerror(errno));

            ::close(sock);
            sock = -1;
            continue;
        }

        // connected :D
        break;
    }

    freeaddrinfo(res0);

    // finish up
    if(sock == -1) {
        throw std::runtime_error(f("Failed to connect: {}", strerror(errno)));
    }

    this->socket = sock;
}

/**
 * Fills in a server TLS configuration.
 */
void ServerConnection::buildTlsConfig(struct tls_config *cfg) {
    int err;

    // Use TLSv1.3 only
    err = tls_config_set_protocols(cfg, TLS_PROTOCOL_TLSv1_3);
    XASSERT(err == 0, "tls_config_set_protocols() failed: {}", tls_config_error(cfg));

    // cubeland protocol
    err = tls_config_set_alpn(cfg, "cubeland/1.0");
    XASSERT(err == 0, "tls_config_set_alpn() failed: {}", tls_config_error(cfg));

    // use secure ciphers only
    err = tls_config_set_ciphers(cfg, "secure");
    XASSERT(err == 0, "tls_config_set_ciphers() failed: {}", tls_config_error(cfg));

    // enable ephemeral Diffie-Hellman keys; this allows forward secrecy
#if NDEBUG
    err = tls_config_set_dheparams(cfg, "auto");
#else
    err = tls_config_set_dheparams(cfg, "none");
#endif
    XASSERT(err == 0, "tls_config_set_dheparams() failed: {}", tls_config_error(cfg));

    // CA certificate path
    const auto caPath = io::PathHelper::resourcesDir() + "/cacert.pem";
    err = tls_config_set_ca_file(cfg, caPath.c_str());
    XASSERT(err == 0, "tls_config_set_ca_path() failed: {}", tls_config_error(cfg));
}


/**
 * Tears down the server connection worker and associated resources.
 */
ServerConnection::~ServerConnection() {
    int err;

    // tell the worker to fuck off
    this->workerRun = false;

    PipeData d(PipeEvent::NoOp);
    err = ::write(this->notePipe[1], &d, sizeof(d));
    if(err == -1) {
        Logging::error("Failed to write shut down request to pipe: {}", strerror(errno));
    }

    this->worker->join();

    // clean up TLS connection
    tls_free(this->client);

    // clsoe the yenpipes
    ::close(this->notePipe[0]);
    ::close(this->notePipe[1]);

    ::close(this->socket);
}



/**
 * Worker main loop
 */
void ServerConnection::workerMain() {
    int err;
    struct pollfd pfd[2];
    PacketHeader hdr;

    // set up
    util::Thread::setName("Server Worker");

    try {
        // process incoming messages and send outgoing ones
        while(this->workerRun) {
            // block on the note pipe (read end)
            pfd[0].fd = this->notePipe[0];
            pfd[0].events = POLLIN;

            // and the raw socket the client is connected to
            pfd[1].fd = this->socket;
            pfd[1].events = POLLIN;

            // block on the client socket and notification pipe
            err = poll(pfd, 2, -1);

            if(err == 0) continue; // timeout expired
            else if(err == -1) {
                throw std::runtime_error(f("poll() failed: {}", strerror(errno)));
            }

            // messages in note pipe?
            if(pfd[0].revents & POLLIN) {
                PipeData d;

                do {
                    err = read(this->notePipe[0], &d, sizeof(d));
                    if(err == -1) {
                        throw std::runtime_error(f("couldn't read yenpipe: {}", strerror(errno)));
                    } else if(!err) {
                        // nothing left in the pipe
                        continue;
                    }

                    // process the event
                    this->workerHandleEvent(d);
                } while(err > 0);
            }

            // try to read packet header
            if(pfd[1].revents & POLLIN) {
readAgain:;
                err = tls_read(this->client, &hdr, sizeof(hdr));
                if(err == TLS_WANT_POLLIN) {
                    goto readAgain;
                } else if(err == TLS_WANT_POLLOUT) {
                    goto readAgain;
                } else if(err == -1) {
                    throw std::runtime_error(f("TLS read failed: {}", tls_error(this->client)));
                } else if(err == 0) {
                    goto beach;
                }

                if(err != sizeof(hdr)) {
                    Logging::error("Partial header read from server: {} bytes, expected {}", err,
                            sizeof(hdr));
                    continue;
                }

                // handle the message
                hdr.length = ntohs(hdr.length);
                hdr.tag = ntohs(hdr.tag);
                this->workerHandleMessage(hdr);
            }

beach:;
        }
    } catch(std::exception &e) {
        Logging::error("Server connection error: {}", e.what());
        // TODO: notify whatever handlers we've got
    }

    // close the connection
closeAgain:;
    err = tls_close(this->client);
    if(err == TLS_WANT_POLLIN || err == TLS_WANT_POLLOUT) {
        goto closeAgain;
    } else if(err == -1) {
        Logging::error("Failed to close server connection: {}", tls_error(this->client));
    }
}

/**
 * Handle a worker yenpipe message.
 */
void ServerConnection::workerHandleEvent(const PipeData &evt) {
    // TODO
}

/**
 * Handles a message received from the server
 */
void ServerConnection::workerHandleMessage(const PacketHeader &hdr) {
    // read the remainder of the packet

    // handle it
}
