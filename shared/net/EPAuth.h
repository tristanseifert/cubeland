/**
 * Messages used by the authentication endpoint
 */
#ifndef SHARED_NET_EPAUTH_H
#define SHARED_NET_EPAUTH_H

#include <uuid.h>
#include <io/Serialization.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>

namespace net::message {

/**
 * Auth endpoint message types
 */
enum AuthMsgType: uint8_t {
    /// client -> server; request authentication
    kAuthRequest                        = 0x01,
    /// server -> client; auth request challenge
    kAuthChallenge                      = 0x02,
    /// client -> server; auth request challenge response
    kAuthChallengeReply                 = 0x03,
    /// server -> client; auth status
    kAuthStatus                         = 0x04,
};

/**
 * Client to server authentication request. This contains the client's ID that's used to look up
 * its signing key.
 */
struct AuthRequest {
    /// client ID
    uuids::uuid clientId;

    template <class Archive> void serialize(Archive &ar) {
        ar(this->clientId);
    }

    AuthRequest() = default;
    AuthRequest(const uuids::uuid &_id) : clientId(_id) {}
};

/**
 * Server to client authentication challenge. The client will sign the provided random data with
 * its auth key and send it back to the server.
 */
struct AuthChallenge {
    /// Length of challenge data in bytes
    constexpr static const size_t kChallengeLength = 32;

    // data to sign for the client
    std::array<std::byte, kChallengeLength> challenge;

    template <class Archive> void serialize(Archive &ar) {
        ar(this->challenge);
    }
};

/**
 * Client to server reply to the authentication challenge
 */
struct AuthChallengeReply {
    // signature over the challenge data
    std::vector<std::byte> signature;

    template <class Archive> void serialize(Archive &ar) {
        ar(this->signature);
    }
};

/**
 * Server reply indicating authentication status
 */
struct AuthStatus {
    /// authentication state (success or an error)
    enum {
        kStatusSuccess,

        /// the player ID is not known
        kStatusUnknownId,
        /// the signature is invalid
        kStatusInvalidSignature,
        /// a temporary server error prevented signature verification
        kStatusTemporaryError,
        /// unknown error
        kStatusUnknownError,
    } state;

    template <class Archive> void serialize(Archive &ar) {
        ar(this->state);
    }
};

}

#endif
