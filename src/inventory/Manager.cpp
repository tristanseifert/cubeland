#include "Manager.h"
#include "Serialization.h"
#include "UI.h"

#include "input/InputManager.h"
#include "io/Format.h"
#include "world/tick/TickHandler.h"
#include "world/WorldSource.h"

#include <Logging.h>
#include <mutils/time/profiler.h>
#include <SDL.h>

#include <cereal/archives/portable_binary.hpp>

#include <sstream>

using namespace inventory;

const std::string Manager::kDataPlayerInfoKey = "inventory.data";

/**
 * Sets up the inventory manager.
 */
Manager::Manager(input::InputManager *_mgr) : in(_mgr) {
    // register a tick handler
    this->saveTickHandler = world::TickHandler::add(std::bind(&Manager::saveTickCallback, this));
}

/**
 * Cleans up the inventory manager, including deallocation of the tick handler.
 */
Manager::~Manager() {
    // remove tick handler
    if(this->saveTickHandler) {
        world::TickHandler::remove(this->saveTickHandler);
    }

    // force saving if dirty
    if(this->inventoryDirty) {
        this->writeInventory();
    }
}

/**
 * Handles an SDL event. This is roughly divided into two states:
 *
 * - Detailed view not open: The "E" key will open the detailed view. 0-9 change active slot.
 * - Detailed view open: The ESC key will close the detailed view.
 */
bool Manager::handleEvent(const SDL_Event &event) {
    // ignore anything that's not a key down event
    if(event.type != SDL_KEYDOWN) return false;

    const auto &k = event.key.keysym;

    if(this->ui->isDetailOpen()) {
        if(k.scancode == SDL_SCANCODE_ESCAPE) {
            this->in->decrementCursorCount();
            this->ui->setDetailOpen(false);
            return true;
        }
    } else {
        // E opens inventory
        if(k.scancode == SDL_SCANCODE_E) {
            this->in->incrementCursorCount();
            this->ui->setDetailOpen(true);
            return true;
        }
        // 0..9 change active slot
        else if(k.sym >= SDLK_0 && k.sym <= SDLK_9) {
            size_t offset = k.sym - SDLK_0;
            this->currentSlot = offset ? (offset - 1) : 9;
            return true;
        }
    }

    // event not handled
    return false;
}



/**
 * Adds `count` occurrences of the block `blockId` to the inventory.
 */
bool Manager::addItem(const uuids::uuid &blockId, const size_t count) {
    if(blockId.is_nil()) return false;

    XASSERT(count && count < kMaxItemsPerSlot, "Invalid count: {}", count);
    LOCK_GUARD(this->slotLock, AddItem);

    // Logging::trace("Add inventory: block {}, count {}", uuids::to_string(blockId), count);

    // check all existing slots to see if we can add one item there
    for(auto &slot : this->slots) {
        // get handle to the block object; also ensure block id matches
        if(!std::holds_alternative<InventoryBlock>(slot)) continue;
        auto &block = std::get<InventoryBlock>(slot);

        if(block.blockId != blockId) continue;

        // compare maximum count
        if(block.count < kMaxItemsPerSlot) {
            block.count += count;
            this->inventoryDirty = true;
            return true;
        }
    }

    // insert a new item
    InventoryBlock block;
    block.blockId = blockId;
    block.count = count;

    for(size_t i = 0; i < this->slots.size(); i++) {
        if(!this->isSlotOccupied(i)) {
            this->slots[i] = block;
            this->inventoryDirty = true;
            return true;
        }
    }

    // no space in inventory
    return false;
}

/**
 * Returns the id of the block in the current slot, if there is a block. Then, decrements the count
 * of that slot by one.
 */
std::optional<uuids::uuid> Manager::dequeueSlotBlock() {
    LOCK_GUARD(this->slotLock, DequeueItem);

    // bail if slot is empty or does not contain a block
    if(!this->isSlotOccupied(this->currentSlot)) return std::nullopt;

    auto &slot = this->slots[this->currentSlot];
    if(!std::holds_alternative<InventoryBlock>(slot)) return std::nullopt;

    // extract its id and decrement its count
    auto &block = std::get<InventoryBlock>(slot);

    if(--block.count == 0) {
        // remove the block
        this->slots[this->currentSlot] = std::monostate();
    }

    // return id
    this->inventoryDirty = true;
    return block.blockId;
}

/**
 * Erases all slots that contain zero-length entries.
 *
 * @note Assumes the slot lock is being held already.
 */
void Manager::removeEmptySlots() {
    for(size_t i = 0; i < this->slots.size(); i++) {
        // ignore actually empty slots
        if(!this->isSlotOccupied(i)) continue;
        auto &slot = this->slots[i];

        // blocks
        if(std::holds_alternative<InventoryBlock>(slot)) {
            auto &block = std::get<InventoryBlock>(slot);
            if(block.count == 0) {
                this->slots[i] = std::monostate();
                this->inventoryDirty = true;
            }
        }
    }
}

/**
 * Loads inventory data for the current player from the given world source.
 *
 * This will store a reference to the source for later, and will write any changes to the inventory
 * back to that world's data store.
 *
 * Loading of data is performed synchronously, but this is ok since we're not going to be in the
 * game loop yet when this is called. At worst, we'll freeze the UI for a couple ms; this is worth
 * looking into when network play is introduced though for UX improvement.
 */
void Manager::loadInventory(std::shared_ptr<world::WorldSource> &world) {
    using namespace internal;

    this->world = world;

    // try to load the inventory data
    auto promise = world->getPlayerInfo(kDataPlayerInfoKey);
    auto value = promise.get_future().get();

    if(value.empty()) {
        return;
    }

    // try to decode inventory data
    std::stringstream stream(std::string(value.begin(), value.end()));
    cereal::PortableBinaryInputArchive arc(stream);

    InventoryData data;
    arc(data);

    // validate loaded data
    if(data.totalSlots > kNumInventorySlots) {
        Logging::error("Refusing to load inventory: too many slots ({})", data.totalSlots);
        return;
    }
    if(data.maxPerSlot > kMaxItemsPerSlot) {
        Logging::error("Refusing to load inventory: too many items per slot ({})", data.maxPerSlot);
        return;
    }

    // restore it
    LOCK_GUARD(this->slotLock, RestoreSlots);
    for(size_t i = 0; i < kNumInventorySlots; i++) {
        // clear slot if no data
        if(!data.slots.contains(i)) {
            this->slots[i] = std::monostate();
            continue;
        }
        const auto &slot = data.slots[i];
        if(std::holds_alternative<std::monostate>(slot)) {
            this->slots[i] = std::monostate();
            continue;
        }

        // handle it according to type
        if(std::holds_alternative<InventoryDataBlockStack>(slot)) {
            const auto &stack = std::get<InventoryDataBlockStack>(slot);

            InventoryBlock block;
            block.blockId = stack.blockId;
            block.count = std::min((size_t) stack.count, kMaxItemsPerSlot);

            this->slots[i] = block;
        }
    }

    // limit selected slot to first row
    this->currentSlot = std::min(9U, data.selectedSlot);
}

/**
 * Serializes inventory data and saves it in the world source.
 */
void Manager::writeInventory() {
    using namespace internal;
    PROFILE_SCOPE(WriteInventory);

    // build the inventory struct
    InventoryData data;
    data.totalSlots = kNumInventorySlots;
    data.maxPerSlot = kMaxItemsPerSlot;
    data.selectedSlot = this->currentSlot;

    {
        LOCK_GUARD(this->slotLock, SerializeSlots);

        for(size_t i = 0; i < kNumInventorySlots; i++) {
            // ignore unoccupied slots
            if(!this->isSlotOccupied(i)) continue;

            // build the appropriate payload struct for each slot
            const auto &slot = this->slots[i];
            if(std::holds_alternative<InventoryBlock>(slot)) {
                const auto &block = std::get<InventoryBlock>(slot);

                InventoryDataBlockStack stack;
                stack.count = block.count;
                stack.blockId = block.blockId;

                data.slots[i] = stack;
            }
        }
    }

    // serialize and compress it
    std::stringstream stream;
    cereal::PortableBinaryOutputArchive arc(stream);
    arc(data);

    const auto str = stream.str();
    std::vector<char> rawBytes(str.begin(), str.end());

    // lastly, write it and clear dirty flag
    auto future = this->world->setPlayerInfo(kDataPlayerInfoKey, rawBytes);
    future.wait();

    this->inventoryDirty = false;
}

/**
 * Tick callback for the background save handler. This will write out inventory data, if needed,
 * at a predefined interval.
 */
void Manager::saveTickCallback() {
    // decrement tick counter until zero
    if(--this->saveTimer) return;

    // write out if needed
    if(this->isDirty() && this->world) {
        this->writeInventory();
    }

    // reset the timer
    this->saveTimer = kSaveDelayTicks;
}
