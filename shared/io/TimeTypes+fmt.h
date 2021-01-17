/**
 * Provides formatting support for system time types.
 */
#ifndef SHARED_FORMAT_TIMETYPES_H
#define SHARED_FORMAT_TIMETYPES_H

#include <sys/time.h>

template <> 
struct fmt::formatter<struct timeval> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if(it != end && *it != '}') {
            throw format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const struct timeval &tv, FormatContext& ctx) {
        double secs = tv.tv_sec;
        secs += (((double)tv.tv_usec) / 1000000.f);
        return format_to(ctx.out(), "{}", secs);
    }
};

#endif
