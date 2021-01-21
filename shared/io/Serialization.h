/**
 * Provides Cereal serialization functions for some commonly used types.
 */
#ifndef IO_SERIALIZATION_H
#define IO_SERIALIZATION_H

#include <cereal/types/array.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
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

/// Archives a 2 component vector
template<class Archive> void serialize(Archive &archive, glm::vec2 &vec) {
    archive(vec.x);
    archive(vec.y);
}
template<class Archive> void serialize(Archive &archive, glm::ivec2 &vec) {
    archive(vec.x);
    archive(vec.y);
}
/// Archives a 3 component vector
template<class Archive> void serialize(Archive &archive, glm::vec3 &vec) {
    archive(vec.x);
    archive(vec.y);
    archive(vec.z);
}
template<class Archive> void serialize(Archive &archive, glm::ivec3 &vec) {
    archive(vec.x);
    archive(vec.y);
    archive(vec.z);
}
/// Archives a 4 component vector
template<class Archive> void serialize(Archive &archive, glm::vec4 &vec) {
    archive(vec.x);
    archive(vec.y);
    archive(vec.z);
    archive(vec.w);
}
}

#endif
