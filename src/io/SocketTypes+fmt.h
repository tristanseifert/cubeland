/**
 * Provides formatting and stringification support for system socket types.
 */
#ifndef PROTO_SOCKETTYPESFMT
#define PROTO_SOCKETTYPESFMT

#include <stdexcept>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

template <> 
struct fmt::formatter<struct sockaddr_storage> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if(it != end && *it != '}') {
            throw format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const struct sockaddr_storage &addr, FormatContext& ctx) {
        char addrStr[64];
        memset(addrStr, 0, sizeof(addrStr));

        int port = 0;

        auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(&addr);
        auto *addr6 = reinterpret_cast<const struct sockaddr_in6 *>(&addr);

        // is it IPv4?
        if(addr.ss_family == AF_INET) {
            if(!inet_ntop(AF_INET, &addr4->sin_addr, addrStr, 64)) {
                throw std::system_error(errno, std::generic_category(),
                        "Failed to stringify IPv4 address");
            }
            port = ntohs(addr4->sin_port);
            return format_to(ctx.out(), "{}:{}", addrStr, port);
        }
        // IPv6?
        else if(addr.ss_family == AF_INET6) {
            if(!inet_ntop(AF_INET6, &addr6->sin6_addr, addrStr, 64)) {
                throw std::system_error(errno, std::generic_category(),
                        "Failed to stringify IPv4 address");
            }
            port = ntohs(addr6->sin6_port);
            return format_to(ctx.out(), "[{}]:{}", addrStr, port);
        }
        // unknown
        else {
            return format_to(ctx.out(), "(sockaddr, family {})", 
                    addr.ss_family);
        }
    }
};


#endif
