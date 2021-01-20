#ifndef NET_HANDLER_AUTH_H
#define NET_HANDLER_AUTH_H

#include "net/PacketHandler.h"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <mutex>

#include <uuid.h>

namespace net::handler {
/**
 * Handles authenticating the client.
 */
class Auth: public PacketHandler {
    public:
        /// reasons authentication may fail
        enum class AuthFailureReason {
            UnknownError,
            UnknownId,
            InvalidSignature,
            TemporaryError,
        };

    public:
        Auth(ServerConnection *_server);
        virtual ~Auth();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void beginAuth();

        /// wait for auth to complete. return success state
        bool waitForAuth();
        /// if auth failed, returns the failure reason
        AuthFailureReason getFailureReason() const {
            return this->failureReason;
        }

    private:
        enum class State {
            /// Currently unauthorized; can accept auth request
            Idle,
            /// Process the received challenge
            SolveChallenge,
            /// Waits for the authentication status to come back from the server
            WaitAuth,
            /// Authentication was successful
            Successful,
            /// Client could NOT be authenticated
            Failed,
        };

    private:
        void handleAuthChallenge(const PacketHeader &, const void *, const size_t);
        void handleAuthStatus(const PacketHeader &, const void *, const size_t);

        void setState(const State);

    private:
        /// current auth state machine state
        State state = State::Idle;
        /// state condition variable
        std::condition_variable stateCond;
        /// state lock
        std::mutex stateLock;

        /// if auth failed, the reason why
        AuthFailureReason failureReason;
        /// tag of the message we expect to receive next
        uint16_t expectedTag;
};
}

#endif
