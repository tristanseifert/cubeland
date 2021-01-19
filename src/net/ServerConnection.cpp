#include "ServerConnection.h"

#include <Logging.h>
#include <io/Format.h>
#include <util/Thread.h>
#include <net/PacketTypes.h>

#include <tls.h>

using namespace net;

/**
 * Creates a new server connection to the specified server.
 *
 * @param host Address or DNS name of the server. The port may be specified as in "host:port" if
 * not using the default.
 */
ServerConnection::ServerConnection(const std::string &host) {
    int err;

    // create a TLS client connection
    this->client = tls_client();
    XASSERT(this->client, "Failed to create TLS client");

    auto config = tls_config_new();
    XASSERT(config, "Failed to allocate TLS config");

    this->buildTlsConfig(config);

    err = tls_configure(this->client, config);
    XASSERT(err == 0, "tls_configure() failed: {}", tls_error(this->client));

    tls_config_free(config);

    // connect it
    static const char *kDefaultPort = "47420";
    const char *portStr = nullptr;

    if(host.find(':') == std::string::npos) {
        portStr = kDefaultPort;
    }

    err = tls_connect(this->client, host.c_str(), portStr);
    if(err) {
        throw std::runtime_error(tls_error(this->client));
    }

    // start our worker thread
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&ServerConnection::workerMain, this);
}

/**
 * Fills in a server TLS configuration.
 */
void ServerConnection::buildTlsConfig(struct tls_config *cfg) {
    int err;

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
}


/**
 * Tears down the server connection worker and associated resources.
 */
ServerConnection::~ServerConnection() {
    int err;

    // tell the worker to fuck off
    this->workerRun = false;
    this->worker->join();

    // clean up TLS connection
    tls_free(this->client);
}



/**
 * Worker main loop
 */
void ServerConnection::workerMain() {
    int err;

    // process incoming messages and send outgoing ones
    while(this->workerRun) {

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
