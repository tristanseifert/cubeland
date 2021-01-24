#include "Auth.h"
#include "Chat.h"
#include "net/Listener.h"
#include "net/ListenerClient.h"

#include "auth/KeyCache.h"

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
 * Initializes the authentication handler/
 */
Auth::Auth(ListenerClient *_client) : PacketHandler(_client) {

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
        // received auth request
        case State::Idle:
            if(header.type != kAuthRequest) {
                throw std::runtime_error("Unexpected packet type");
            }
            this->handleAuthReq(header, payload, payloadLen);
            break;

        // we've sent a challenge to the client; expect a reply
        case State::VerifyChallenge:
            if(header.type != kAuthChallengeReply) {
                throw std::runtime_error("Unexpected packet type");
            }
            this->handleAuthChallengeReply(header, payload, payloadLen);
            break;

        // handle some other messages when authenticated
        case State::Successful:
            switch(header.type) {
                case kAuthGetConnected:
                    this->getConnectedUsers(header, payload, payloadLen);
                    break;

                default:
                    throw std::runtime_error(f("Unhandled auth packet type: ${:02x}", header.type));
                    break;
            }
            break;

        // all other states
        default:
            throw std::runtime_error(f("Unhandled auth state: {}", this->state));
            break;
    }
}



/**
 * Produces a list of all connected users.
 */
void Auth::getConnectedUsers(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // deserialize the request
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    AuthGetUsersRequest request;
    iArc(request);

    // for each connected client, add an entry to the users list
    const bool includeAddr = request.includeAddress;
    AuthGetUsersReply reply;

    this->client->getListener()->forEach([&, includeAddr](auto &client) {
        // skip if user isn't authenticated
        const auto userId = client->getClientId();
        if(!userId) {
            reply.numUnauthenticated++;
            return;
        }

        AuthUserInfo info;

        info.userId = *userId;
        info.displayName = *client->getClientDisplayName();

        if(includeAddr) {
            info.remoteAddr = f("{}", client->getClientAddr());
        }

        reply.users.push_back(info);
    });

    // send response
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    oArc(reply);

    this->client->writePacket(kEndpointAuthentication, kAuthGetConnectedReply, oStream.str(),
            header.tag);
}




/**
 * Handles an authentication request packet.
 */
void Auth::handleAuthReq(const PacketHeader &header, const void *payload, const size_t payloadLen) {
    // attempt to deserialize the auth request packet
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    AuthRequest request;
    iArc(request);

    this->clientId = request.clientId;
    this->displayName = request.displayName;

    // generate data for the challenge (using arc4random_buf or LibreSSL)
    std::array<std::byte, AuthChallenge::kChallengeLength> random;

#if 1
    arc4random_buf(random.data(), random.size());
#else
    RAND_bytes(reinterpret_cast<unsigned char *>(random.data()), random.size());
#endif

    // build challenge response and serialize
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);

    AuthChallenge challenge(random);
    oArc(challenge);

    // update state machine
    this->state = State::VerifyChallenge;
    this->challengeData = random;

    // send it
    this->client->writePacket(kEndpointAuthentication, kAuthChallenge, oStream.str(), header.tag);
}

/**
 * Handles a client's response to a previous authentication challenge. This is really just a simple
 * signature verification using the client's public key... which we may need to fetch from the
 * web service.
 */
void Auth::handleAuthChallengeReply(const PacketHeader &header, const void *payload,
        const size_t payloadLen) {
    bool valid = false;

    // Deserialize response
    std::stringstream stream(std::string(reinterpret_cast<const char *>(payload), payloadLen));
    cereal::PortableBinaryInputArchive iArc(stream);

    AuthChallengeReply reply;
    iArc(reply);

    // verify the challenge
    auto clientKey = auth::KeyCache::get(this->clientId);

    try {
        valid = util::Signature::verify(clientKey, this->challengeData.data(),
                this->challengeData.size(), reply.signature);
    } catch(std::exception &e) {
        Logging::error("Failed to verify challenge response: {}", e.what());
        valid = false;
    }

    // send appropriate response
    AuthStatus status;

    if(valid) {
        status.state = AuthStatus::kStatusSuccess;
        this->state = State::Successful;
    } else {
        status.state = AuthStatus::kStatusInvalidSignature;
        this->state = State::Failed;
    }

    Logging::trace("Client {} (name '{}') auth state: {}", this->client->getClientAddr(), 
            this->displayName, valid ?  "success" : "failure");

    // send the response
    std::stringstream oStream;
    cereal::PortableBinaryOutputArchive oArc(oStream);
    oArc(status);

    this->client->writePacket(kEndpointAuthentication, kAuthStatus, oStream.str(), header.tag);

    // invoke handlers
    this->client->authStateChanged();

    // only if the player was successfully authenticated, send a message indicating they've joined
    Chat::playerJoined(this->clientId, this->displayName);
}
