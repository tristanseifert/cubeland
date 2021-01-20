#ifndef SHARED_NET_PACKETTYPES_H
#define SHARED_NET_PACKETTYPES_H

#include <cstdint>

#include "EPAuth.h"

// pack structs
#pragma pack(push, 1)

namespace net {

/**
 * Packet endpoints
 */
enum PacketEndpoint: uint8_t {
    /// Utility/helpers (ping, MoTD)
    kEndpointUtility                    = 0x01,
    /// User authentication
    kEndpointAuthentication             = 0x02,
    /// Block change
    kEndpointBlockChange                = 0x03,
    /// Chunk requests
    kEndpointChunk                      = 0x04,
    /// Chat messages
    kEndpointChat                       = 0x05,
    /// Player data updates
    kEndpointPlayerData                 = 0x06,
    /// Player movement
    kEndpointPlayerMovement             = 0x07,

};

/**
 * Header for all network packets
 */
struct PacketHeader {
    /// Endpoint
    uint8_t endpoint;
    /// Packet minor type
    uint8_t type;

    /// tag (used for responses)
    uint16_t tag;
    /// length of packet (in units of 4 bytes) in network byte order
    uint16_t length;

    // reserved; send as 0
    uint16_t reserved;

    /// payload data
    char payload[];
};

}

// restore packing mode
#pragma pack(pop)

#endif
