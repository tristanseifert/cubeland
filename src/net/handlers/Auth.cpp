#include "Auth.h"
#include "net/ServerConnection.h"

#include "web/AuthManager.h"

#include <net/PacketTypes.h>
#include <net/EPAuth.h>

#include <Logging.h>
#include <io/Format.h>
#include <util/Signature.h>

#include <openssl/rand.h>
#include <cereal/archives/portable_binary.hpp>

#include <cstdlib>
#include <sstream>

using namespace net::handler;
using namespace net::message;

/**
 * Initializes the auth handler.
 */
Auth::Auth(ServerConnection *_server) : PacketHandler(_server) {

}

/**
 * If anyone is still waiting on auth when we're deallocing, release them.
 */
Auth::~Auth() {
    std::lock_guard<std::mutex> lg(this->stateLock);

    this->state = State::Failed;
    this->stateCond.notify_all();
}


/**
 * We handle all auth endpoint packets.
 */
bool Auth::canHandlePacket(const PacketHeader &header) {
    // it must be the auth endpoint
    if(header.endpoint != kEndpointAuthentication) return false;
    // it musn't be more than the max value
    if(header.type >= kAuthTypeMax) return false;

    return true;
}


/**
 * Handles an auth packet.
 */
void Auth::handlePacket(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    switch(this->state) {
        // we've sent an auth challenge; assume this is a challenge to solve
        case State::SolveChallenge:
            if(header.type != kAuthChallenge) {
                throw std::runtime_error("Unexpected packet type");
            } else if(header.tag != this->expectedTag) {
                throw std::runtime_error(f("Unexpected tag (${:04x}, expected ${:04x})", header.tag,
                            this->expectedTag));
            }
            this->handleAuthChallenge(header, payload, payloadLen);
            break;

        // after sending a challenge, expect an auth status message
        case State::WaitAuth:
            if(header.type != kAuthStatus) {
                throw std::runtime_error("Unexpected packet type");
            } else if(header.tag != this->expectedTag) {
                throw std::runtime_error(f("Unexpected tag (${:04x}, expected ${:04x})", header.tag,
                            this->expectedTag));
            }
            this->handleAuthStatus(header, payload, payloadLen);
            break;


        // all other packets
        default:
            throw std::runtime_error(f("Unhandled auth state: {}", this->state));
            break;
    }
}

/**
 * Handles an auth challenge received from the server.
 */
void Auth::handleAuthChallenge(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    std::stringstream oStream;

    // deserialize challenge
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    AuthChallenge challenge;
    iArc(challenge);

    // sign this data with our auth key
    const auto &random = challenge.challenge;
    std::vector<std::byte> signature;

    web::AuthManager::sign(random.data(), random.size(), signature);

    // build response packet and send it
    AuthChallengeReply reply(signature);

    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(reply);

    this->setState(State::WaitAuth);
    this->expectedTag = this->server->writePacket(kEndpointAuthentication, kAuthChallengeReply,
            oStream.str());
}

/**
 * Handles an authentication status message from the server. This'll determine whether we enter
 * the failure or success states.
 */
void Auth::handleAuthStatus(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    // deserialize status
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    AuthStatus status;
    iArc(status);

    Logging::debug("Auth status: {}", status.state);

    // handle it
    switch(status.state) {
        // authentication succeeded
        case AuthStatus::kStatusSuccess:
            this->setState(State::Successful);
            return;

        case AuthStatus::kStatusUnknownId:
            this->failureReason = AuthFailureReason::UnknownId;
            break;
        case AuthStatus::kStatusInvalidSignature:
            this->failureReason = AuthFailureReason::InvalidSignature;
            break;
        case AuthStatus::kStatusTemporaryError:
            this->failureReason = AuthFailureReason::TemporaryError;
            break;

        // unknown auth type. assume error
        default:
            this->failureReason = AuthFailureReason::UnknownError;
            Logging::warn("Unknown auth status {}; treating as failure", status.state);
            break;
    }

    // everything EXCEPT success will fall through here
    this->setState(State::Failed);
}

/**
 * Updates state machine
 */
void Auth::setState(const State newState) {
    std::lock_guard<std::mutex> lg(this->stateLock);

    this->state = newState;
    this->stateCond.notify_all();
}



/**
 * Starts the authentication process. This sends the initial "auth request" packet.
 */
void Auth::beginAuth() {
    XASSERT(this->state == State::Idle, "Invalid state: {}", this->state);

    std::stringstream stream;

    // build the auth request packet
    net::message::AuthRequest req(web::AuthManager::getPlayerId());
    cereal::PortableBinaryOutputArchive arc(stream);
    arc(req);

    // send it
    this->setState(State::SolveChallenge);
    this->expectedTag = this->server->writePacket(kEndpointAuthentication, kAuthRequest,
            stream.str());
}

/**
 * Wait for authentication state to be either success or failure
 */
bool Auth::waitForAuth() {
    // check the meepage
    std::unique_lock<std::mutex> lk(this->stateLock);
    this->stateCond.wait(lk, [&]{
        return (this->state == State::Successful) || (this->state == State::Failed);
    });

    return (this->state == State::Successful);
}
