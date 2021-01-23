#ifndef SHARED_NET_EPTIME_H
#define SHARED_NET_EPTIME_H

#include <cstdint>

#include <cereal/access.hpp>

namespace net::message {
/**
 * Time endpoint message types
 */
enum TimeMsgType: uint8_t {
    /// server -> client; sent upon connection with the time parameters
    kTimeInitialState                   = 0x01,
    /// server -> client; periodic time updates
    kTimeUpdate                         = 0x02,

    kTimeTypeMax,
};

/**
 * Initial time state connection message. This indicates to clients the tick speed, e.g. the factor
 * to convert wall clock time (in seconds) to in-game time.
 */
struct TimeInitialState {
    /// current game time
    double currentTime;
    /// factor to go from seconds to in game ticks
    double tickFactor;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->currentTime);
            ar(this->tickFactor);
        }
};

/**
 * Periodic unsolicited time updates. The server will periodically send these so clients can re-
 * synchronize themselves.
 */
struct TimeUpdate {
    /// current time
    double currentTime;

    private:
        friend class cereal::access;
        template <class Archive> void serialize(Archive &ar) {
            ar(this->currentTime);
        }
};

}

#endif
