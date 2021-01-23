#ifndef NET_HANDLER_TIME_H
#define NET_HANDLER_TIME_H

#include "net/PacketHandler.h"

#include <cstdint>

namespace net::handler {

class Time: public PacketHandler {
    friend class net::ServerConnection;

    public:
        Time(ServerConnection *_server) : PacketHandler(_server) {};
        virtual ~Time() = default;

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

    private:
        void configTime(const PacketHeader &, const void *, const size_t);
        void resyncTime(const PacketHeader &, const void *, const size_t);

    private:
        /// time factor (the speed at which the world time changes)
        double timeFactor = 0.;
        /// last synced time
        double lastSyncTime = 0;
};
}

#endif
