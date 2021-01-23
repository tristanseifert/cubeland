#ifndef NET_HANDLER_TIME_H
#define NET_HANDLER_TIME_H

#include "net/PacketHandler.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include <cpptime.h>
#include <cereal/access.hpp>

namespace net::handler {
/**
 * Updates clients as to what tf the time is
 */
class Time: public PacketHandler {
    public:
        Time(ListenerClient *_client);
        virtual ~Time();

        bool canHandlePacket(const PacketHeader &header) override;
        void handlePacket(const PacketHeader &header, const void *payload,
                const size_t payloadLen) override;

        void authStateChanged() override;

        void sendTime();

    private:
        /// thymer for all clients
        static CppTime::Timer timer;
        /// number of connected clients; if none are connected, time doesn't advance
        static std::atomic_uint numConnectedClients;

        /// timer for notifying the client of the current thyme
        CppTime::timer_id updateTimer;
};
}

#endif
