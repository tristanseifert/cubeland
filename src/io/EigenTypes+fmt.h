/**
 * Provides formatting support for Eigen types
 */
#ifndef SHARED_FORMAT_EIGENTYPES_H
#define SHARED_FORMAT_EIGENTYPES_H

#include <Eigen/Eigen>

template <>
struct fmt::formatter<Eigen::Vector3d> {
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if(it != end && *it != '}') {
            throw format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const Eigen::Vector3d &v, FormatContext& ctx) {
        return format_to(ctx.out(), "({}, {}, {})", v(0), v(1), v(2));
    }
};

#endif
