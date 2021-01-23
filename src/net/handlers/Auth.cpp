#include "Auth.h"
#include "net/ServerConnection.h"

#include "web/AuthManager.h"
#include "io/PrefsManager.h"

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
    std::lock_guard<std::mutex> lg(this->stateLock), lg2(this->requestsLock);

    this->state = State::Failed;
    this->stateCond.notify_all();

    for(auto &[key, promise] : this->requests) {
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Auth handler going away")));
    }
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
            switch(header.type) {
                case kAuthGetConnectedReply:
                    this->connectedReply(header, payload, payloadLen);
                    break;

                default:
                    throw std::runtime_error(f("Unhandled auth state: {}", this->state));
                    break;
            }

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

    req.displayName = io::PrefsManager::getString("auth.displayName", "Mystery Player");

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



/**
 * Requests from the server a list of all connected players.
 */
std::future<std::vector<Auth::Player>> Auth::getConnectedPlayers(const bool wantClientAddr) {
    std::promise<std::vector<Player>> prom;
    auto future = prom.get_future();

    // build request packet
    AuthGetUsersRequest req;

    req.includeAddress = wantClientAddr;

    std::stringstream stream;
    cereal::PortableBinaryOutputArchive arc(stream);
    arc(req);

    // send it and save the promise
    std::lock_guard<std::mutex> lg(this->requestsLock);
    const auto tag = this->server->writePacket(kEndpointAuthentication, kAuthGetConnected,
            stream.str());
    this->requests[tag] = std::move(prom);

    return future;
}

/**
 * Handles a response to the player listing request.
 */
void Auth::connectedReply(const PacketHeader &hdr, const void *payload, const size_t payloadLen) {
    try {
        // deserialize message
        std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
        cereal::PortableBinaryInputArchive iArc(stream);

        AuthGetUsersReply reply;
        iArc(reply);

        // convert the returned objects
        std::vector<Player> list;

        for(const auto &in : reply.users) {
            Player p;

            p.id = in.userId;
            p.displayName = in.displayName;
            p.remoteAddr = in.remoteAddr;

            list.push_back(p);
        }

        // invoke promise
        std::lock_guard<std::mutex> lg(this->requestsLock);
        this->requests.at(hdr.tag).set_value(list);
        this->requests.erase(hdr.tag);
    } catch(std::exception &e) {
        Logging::error("Failed to process auth user list: {}", e.what());

        std::lock_guard<std::mutex> lg(this->requestsLock);
        this->requests.at(hdr.tag).set_exception(std::current_exception());
        this->requests.erase(hdr.tag);
    }
}

