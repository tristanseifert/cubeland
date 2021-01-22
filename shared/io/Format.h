/**
 * Umbrella header for formatting stuff. Includes format helpers for various
 * common types.
 */
#ifndef FORMAT_H
#define FORMAT_H

// include fmtlib and define our format shortcut
#include <fmt/format.h>

#include <utility>
template <typename... Args>
inline auto f(Args&&... args) -> decltype(fmt::format(std::forward<Args>(args)...)) {
    return fmt::format(std::forward<Args>(args)...);
}

// hex dump support
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#define hexdump(a,b) spdlog::to_hex(a,b) 

// formatting support for socket addresses
#include "SocketTypes+fmt.h"
#include "TimeTypes+fmt.h"
#include "GlmTypes+fmt.h"

// uuids support
#include <uuid.h>

// optionals
template <class T>
struct fmt::formatter<std::optional<T>> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if(it != end && *it != '}') {
            throw format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const std::optional<T> &opt, FormatContext& ctx) {
        if(opt) {
            return format_to(ctx.out(), "Optional({})", *opt);
        } else {
            return format_to(ctx.out(), "Optional(null)");
        }
    }
};

// uuids
template <> 
struct fmt::formatter<struct uuids::uuid> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if(it != end && *it != '}') {
            throw format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const struct uuids::uuid &id, FormatContext& ctx) {
        return format_to(ctx.out(), "{}", uuids::to_string(id));
    }
};
#endif
