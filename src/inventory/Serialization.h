/**
 * Provides types for the serialization of inventory data into worlds.
 */
#ifndef INVENTORY_SERIALIZATION_H
#define INVENTORY_SERIALIZATION_H

#include "io/Serialization.h"

#include <variant>
#include <unordered_map>
#include <cstdint>

#include <uuid.h>

#include <cereal/types/variant.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>

namespace inventory::internal {

/**
 * Represents a stack of one or more blocks in inventory.
 */
struct InventoryDataBlockStack {
    friend class cereal::access;

    /// number of blocks
    uint32_t count;
    /// block ID
    uuids::uuid blockId;

    private:
        template<class Archive> void serialize(Archive &archive) {
            archive(this->count);
            archive(this->blockId);
        }

};

/**
 * An instance of this struct is the contents of what's written to the world player info key
 * `inventory.data`.
 */
struct InventoryData {
    friend class cereal::access;

    using SlotType = std::variant<std::monostate, InventoryDataBlockStack>;

    /// total number of inventory slots
    uint32_t totalSlots;
    /// maximum items per slot
    uint32_t maxPerSlot;
    /// occupied inventory slots
    std::unordered_map<uint32_t, SlotType> slots;

    private:
        // cerialization
        template<class Archive> void serialize(Archive &archive) {
            archive(totalSlots);
            archive(maxPerSlot);
            archive(slots);
        }
};

}

#endif
