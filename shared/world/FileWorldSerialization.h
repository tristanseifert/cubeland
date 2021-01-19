/**
 * Defines types used to serialize data into/out of the world file.
 */
#ifndef WORLD_FILEWORLDSERIALIZATION_H
#define WORLD_FILEWORLDSERIALIZATION_H

#include "chunk/Chunk.h"

#include <string>
#include <utility>
#include <unordered_map>
#include <variant>
#include <cstdint>

#include <cereal/types/variant.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/portable_binary.hpp>

namespace world {
/**
 * Properties struct written for a particular slice. It contains (X,Z) tuples mapping to a map of
 * string to value types.
 *
 * The point type is ordered as 0xZZXX, so that when sorted, a row's items are sequentially. This
 * improves cache locality when we also traverse the array in sequence with it :D
 */
struct ChunkSliceFileBlockMeta {
    friend class cereal::access;

    using PointType = uint16_t; // high 8 bits = Z, low 8 bits = X
    using ValueType = std::variant<std::monostate, bool, std::string, double, int64_t>;
    using PropsType = std::unordered_map<std::string, ValueType>;

    std::unordered_map<PointType, PropsType> properties;

    private:
        // cerialization
        template<class Archive> void serialize(Archive & archive) {
            archive(properties);
        }
};
}

#endif
