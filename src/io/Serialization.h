/**
 * Provides Cereal serialization functions for some commonly used types.
 */
#ifndef IO_SERIALIZATION_H
#define IO_SERIALIZATION_H

#include <cereal/types/array.hpp>

#include <uuid.h>

#include <cstddef>
#include <algorithm>

namespace cereal {
/// Archives the UUID by saving its 16 content bytes.
template<class Archive> void save(Archive &archive, uuids::uuid const &uuid) { 
    std::array<std::byte, 16> bytes;

    const auto span = uuid.as_bytes();
    std::copy(span.begin(), span.end(), std::begin(bytes));
    archive(bytes);
}
/// Recreates an UUID by unarchiving its 16 content bytes.
template<class Archive> void load(Archive &archive, uuids::uuid &uuid) {
    std::array<uuids::uuid::value_type, 16> read;
    archive(read);

    uuid = uuids::uuid(read);
}
}

#endif
