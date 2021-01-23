/**
 * Messages used by the authentication endpoint
 */
#ifndef SHARED_NET_EPAUTH_H
#define SHARED_NET_EPAUTH_H

#include <uuid.h>
#include <io/Serialization.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
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

    /// client -> server; get list of connected users
    kAuthGetConnected                   = 0x05,
    /// server -> client; returns list of connected users
    kAuthGetConnectedReply              = 0x06,

    kAuthTypeMax,
};

/**
 * Client to server authentication request. This contains the client's ID that's used to look up
 * its signing key.
 */
struct AuthRequest {
    /// client ID
    uuids::uuid clientId;
    /// the display name we'll use
    std::string displayName;

    AuthRequest() = default;
    AuthRequest(const uuids::uuid &_id) : clientId(_id) {}

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->clientId);
            ar(this->displayName);
        }
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
    AuthChallenge() {
        std::fill(this->challenge.begin(), this->challenge.end(), std::byte(0));
    }
    AuthChallenge(const std::array<std::byte, kChallengeLength> &_bytes) : challenge(_bytes) {}

    private:
        friend class cereal::access;
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

    AuthChallengeReply() = default;
    AuthChallengeReply(const std::vector<std::byte> &_sig) : signature(_sig) {}

    private:
        friend class cereal::access;
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
        kStatusSuccess                  = 1,

        /// the player ID is not known
        kStatusUnknownId                = 0x80,
        /// the signature is invalid
        kStatusInvalidSignature,
        /// a temporary server error prevented signature verification
        kStatusTemporaryError,
        /// unknown error
        kStatusUnknownError,
    } state = kStatusUnknownError;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->state);
        }
};



/**
 * Request for listing all connected users
 */
struct AuthGetUsersRequest {
    /// whether the client addresses should be included. server is not required to honor this
    bool includeAddress = false;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->includeAddress);
        }
};

/**
 * Info on a single connected user
 */
struct AuthUserInfo {
    /// user's global ID
    uuids::uuid userId;
    /// display name for user
    std::string displayName;

    /// if requested (and allowed,) stringified player connecting address
    std::optional<std::string> remoteAddr;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->userId);
            ar(this->displayName);
            ar(this->remoteAddr);
        }
};

/**
 * Reply to a request for all connected users.
 */
struct AuthGetUsersReply {
    /// number of connections that haven't authenticated yet
    uint32_t numUnauthenticated = 0;

    /// all authenticated users
    std::vector<AuthUserInfo> users;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->numUnauthenticated);
            ar(this->users);
        }
};

}

#endif
