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

#endif
