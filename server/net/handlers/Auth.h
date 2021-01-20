#ifndef NET_HANDLER_AUTH_H
#define NET_HANDLER_AUTH_H

#include "net/PacketHandler.h"

#include <array>
#include <cstddef>
#include <optional>

#include <uuid.h>

namespace net::handler {
class Auth: public PacketHandler {
    public:
        Auth(ListenerClient *_client);
        virtual ~Auth() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        /// whether the authentication process succeeded
        const bool isAuthenticated() const {
            return (this->state == State::Successful);
        }
        /// if authenticated, the ID of the client
        const std::optional<uuids::uuid> getClientId() const {
            if(this->isAuthenticated()) {
                return this->clientId;
            }
            return std::nullopt;
        }

    private:
        enum class State {
            /// no authentication took place. accept auth requests
            Idle,
            /// a challenge has been sent; verify it
            VerifyChallenge,
            /// Authentication was successful
            Successful,
            /// Client could NOT be authenticated
            Failed,
        };

    private:
        void handleAuthReq(const PacketHeader &, const void *, const size_t);
        void handleAuthChallengeReply(const PacketHeader &, const void *, const size_t);

    private:
        /// current auth state machine state
        State state = State::Idle;

        /// ID of the client (from first auth request packet)
        uuids::uuid clientId;
        /// random data generated for client auth challenge
        std::array<std::byte, 32> challengeData;
};
}

#endif
