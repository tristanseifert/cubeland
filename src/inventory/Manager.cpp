#include "Manager.h"
#include "UI.h"

#include "input/InputManager.h"

#include <Logging.h>
#include <SDL.h>

using namespace inventory;

/**
 * Sets up the inventory manager.
 */
Manager::Manager(input::InputManager *_mgr) : in(_mgr) {

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
    XASSERT(count && count < kMaxItemsPerSlot, "Invalid count: {}", count);

    // Logging::trace("Add inventory: block {}, count {}", uuids::to_string(blockId), count);

    // check all existing slots to see if we can add one item there
    for(auto &slot : this->slots) {
        // get handle to the block object
        if(!std::holds_alternative<InventoryBlock>(slot)) continue;
        auto &block = std::get<InventoryBlock>(slot);

        // compare maximum count
        if(block.count < kMaxItemsPerSlot) {
            block.count += count;
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
    return block.blockId;
}

