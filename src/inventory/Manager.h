/**
 * Manages the player's inventory.
 */
#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

union SDL_Event;

#include <array>
#include <variant>
#include <optional>
#include <memory>
#include <mutex>
#include <cstddef>

#include <uuid.h>

namespace input {
class InputManager;
}

namespace world {
class ClientWorldSource;
}

namespace inventory {
class UI;
class UIBar;
class UIDetail;

class Manager {
    friend class UI;
    friend class UIBar;
    friend class UIDetail;

    public:
        Manager(input::InputManager *input);
        ~Manager();

        bool handleEvent(const SDL_Event &);

        /// Returns the index of the currently selected slot
        size_t getSelectedSlot() const {
            return this->currentSlot;
        }
        /// Sets the currently selected slot
        void setSelectedSlot(const size_t slot) {
            this->currentSlot = (slot % 10);
        }

        /// Checks whether the given inventory slot holds any objects
        bool isSlotOccupied(const size_t slot) const {
            return !std::holds_alternative<std::monostate>(this->slots[slot]);
        }

        /// Inserts the given block to inventory, if there is sufficient space
        bool addItem(const uuids::uuid &blockId, const size_t count = 1);
        /// If the current slot contains blocks, returns its id and decrements its count by 1
        std::optional<uuids::uuid> dequeueSlotBlock();

        /// Returns the dirty state of inventory data. Use this as an indicator to write it out.
        const bool isDirty() const {
            return this->inventoryDirty;
        }
        /// Marks inventory as dirty.
        void markDirty() {
            this->inventoryDirty = true;
        }

        /// Loads inventory data from the given world source
        void loadInventory(std::shared_ptr<world::ClientWorldSource> &world);
        /// Writes inventory data to the previously opened world source.
        void writeInventory();

    public:
        /// total number of inventory slots
        constexpr static const size_t kNumInventorySlots = 60;
        /// maximum number of items per inventory slot
        constexpr static const size_t kMaxItemsPerSlot = 99;

        static_assert(kNumInventorySlots % 10 == 0, "Number of inventory slots must be multiple of 10");

    private:
        /// Number of ticks between inventory saves
        constexpr static const size_t kSaveDelayTicks = 60; // (60 * 25ms = 1.5 sec)
        /// Player info key for the inventory data
        static const std::string kDataPlayerInfoKey;

    private:
        struct InventoryBlock {
            /// block ID
            uuids::uuid blockId;
            /// count
            size_t count = 0;
        };

        using InventoryType = std::variant<std::monostate, InventoryBlock>;

    private:
        void removeEmptySlots();

        void saveTickCallback();

    private:
        UI *ui = nullptr;
        input::InputManager *in = nullptr;

        /// lock protecting access to the slot data
        std::mutex slotLock;
        /// currently selected slot
        size_t currentSlot = 0;
        /// storage for inventory data
        std::array<InventoryType, kNumInventorySlots> slots;

        /// when set, the inventory data is dirty and should be written out again
        bool inventoryDirty = false;
        /// world source from which we read inventory data, and to which inventory data is written
        std::shared_ptr<world::ClientWorldSource> world = nullptr;

        /// save tick handler
        uint32_t saveTickHandler = 0;
        /// counter for inventory saving
        size_t saveTimer = kSaveDelayTicks;
};
}

#endif
