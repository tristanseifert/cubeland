#include "UIDetail.h"
#include "UI.h"
#include "ItemDrawing.h"
#include "Manager.h"

#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"
#include "world/block/TextureLoader.h"
#include "gui/GameUI.h"
#include "gfx/gl/texture/Texture2D.h"

#include <mutils/time/profiler.h>
#include <Logging.h>

#include <imgui.h>

#include <algorithm>

using namespace inventory;

/**
 * Loads textures needed to display the inventory UI.
 */
UIDetail::UIDetail(UI *_owner) : owner(_owner) {
    this->deleteSlotTex = world::BlockRegistry::registerTexture(
            world::BlockRegistry::TextureType::kTypeInventory, glm::ivec2(96, 96), [](auto &out) {
        world::TextureLoader::load("inventory/detail/delete.png", out);
    });
}

/**
 * Draws the main window of the detail view.
 *
 * Note that this is called from within BeginPopupModal() context already, so there is no need to
 * create (or end) the window to hold the view.
 */
void UIDetail::draw(gui::GameUI *ui) {
    PROFILE_SCOPE(InventoryWindowDraw);

    // context menu for the title bar
    if(ImGui::BeginPopupContextItem()) {
        ImGui::MenuItem("Show Registered Items", nullptr, &this->showsRegisteredItems);

        ImGui::EndPopup();
    }

    // main inventory area
    const glm::vec2 mainSize((ItemDrawing::kItemSize * 11.5), (ItemDrawing::kItemSize * 7.33));
    if(ImGui::BeginChild("Inventory Contents", mainSize, false, ImGuiWindowFlags_NoScrollbar)) {
        // trash and actions
        this->drawDeleteItem(ImGui::GetCursorScreenPos(), ui);

        ImGui::Dummy(glm::vec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(glm::vec2(0, 5));

        // remaining inventory rows
        for(size_t off = 10; off < Manager::kNumInventorySlots; off += 10) {
            this->drawRow(ui, off);
            ImGui::NewLine();
        }

        // draw the inventory row that's displayed in the bar
        ImGui::Dummy(glm::vec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(glm::vec2(0, 5));

        this->drawRow(ui, 0);
    }
    ImGui::EndChild();

    // if enabled, draw the list of all registered blocks
    if(this->showsRegisteredItems) {
        ImGui::SameLine();

        const glm::vec2 registeredWidth(kRegisteredItemsWidth, 0);

        if(ImGui::BeginChild("Registered Items", registeredWidth, false, ImGuiWindowFlags_NoScrollbar)) {
            this->displayRegisteredItemsWindow(ui);
        }
        ImGui::EndChild();
    }
}

/**
 * Draws a row of 10 items.
 */
void UIDetail::drawRow(gui::GameUI *gui, const size_t offset) {
    for(size_t i = offset; i < offset + 10; i++) {
        ImGui::PushID(i);

        glm::vec2 pos = ImGui::GetCursorScreenPos();
        const bool occupied = this->owner->inventory->isSlotOccupied(i);

        ItemDrawing::drawItemBackground(pos);

        if(occupied) {
            this->owner->drawItem(pos, i);
        }

        // spacing
        ImGui::Dummy(glm::vec2(ItemDrawing::kItemSize));

        // various behaviors only for occupied slots
        if(occupied) {
            // drag source (if the slot is NOT empty)
            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                SlotDragPayload p;
                p.slot = i;

                if(ImGui::GetIO().KeyShift) {
                    p.modifiers = kSplitStack;
                }

                ImGui::SetDragDropPayload(kInventorySlotDragType, &p, sizeof(p));

                this->dragTooltipForItem(p);
                ImGui::EndDragDropSource();
            }
        }
        // all slots may become drop targets
        if(ImGui::BeginDragDropTarget()) {
            // accept inventory slot data
            if(const auto payload = ImGui::AcceptDragDropPayload(kInventorySlotDragType)) {
                XASSERT(payload->DataSize == sizeof(SlotDragPayload), "Invalid paylad size {}", payload->DataSize);

                this->handleItemDrop(i, reinterpret_cast<SlotDragPayload *>(payload->Data));
            }
            // accept blocks from the registry
            else if(const auto payload = ImGui::AcceptDragDropPayload(kRegisteredBlockDragType)) {
                XASSERT(payload->DataSize == sizeof(RegisteredBlockDragPayload), "Invalid paylad size {}", payload->DataSize);

                this->handleItemDrop(i, reinterpret_cast<RegisteredBlockDragPayload *>(payload->Data));
            }

            // end drop target
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::PopID();
    }
}

/**
 * Draws a drag tooltip for an inventory slot. This contains the image, type, and count.
 */
void UIDetail::dragTooltipForItem(const SlotDragPayload &payload) {
    LOCK_GUARD(this->owner->inventory->slotLock, DrawDragTooltip);
    const auto &slot = this->owner->inventory->slots[payload.slot];
    size_t count = 0;
    std::string name;

    // does the slot contain a block?
    if(std::holds_alternative<Manager::InventoryBlock>(slot)) {
        const auto &block = std::get<Manager::InventoryBlock>(slot);
        auto bo = world::BlockRegistry::getBlock(block.blockId);

        count = block.count;
        name = bo->getInternalName();

        ItemDrawing::drawBlockItem(ImGui::GetCursorScreenPos(), block.blockId);
    }

    // image (this was drawn earlier so this is just a dummy)
    ImGui::Dummy(glm::vec2(ItemDrawing::kItemSize - 2));
    ImGui::SameLine();

    // counts
    if(count) {
        ImGui::Text("%zux %s\nFrom Inventory Slot %zu", count, name.c_str(), payload.slot + 1);
    } else {
        ImGui::Text("1x %s\nFrom Inventory Slot %zu", name.c_str(), payload.slot + 1);
    }

    // modifiers
    if(payload.modifiers & kSplitStack) {
        ImGui::Text("Splitting stack on drop");
    }
}

/**
 * Draws a drag tooltip for a block dragged from the registered blocks list.
 */
void UIDetail::dragTooltipForItem(const RegisteredBlockDragPayload &payload) {
    // image (this was drawn earlier so this is just a dummy)
    ItemDrawing::drawBlockItem(ImGui::GetCursorScreenPos(), payload.blockId);

    ImGui::Dummy(glm::vec2(ItemDrawing::kItemSize - 2));
    ImGui::SameLine();

    // block name
    auto bo = world::BlockRegistry::getBlock(payload.blockId);

    ImGui::Text("%zux %s", Manager::kMaxItemsPerSlot, bo->getInternalName().c_str());
}



/**
 * Handles an accepted drop of an inventory slot.
 */
void UIDetail::handleItemDrop(const size_t dstSlot, const SlotDragPayload *p) {
    LOCK_GUARD(this->owner->inventory->slotLock, HandleSlotDrop);

    // get source slot info
    auto &source = std::get<Manager::InventoryBlock>(this->owner->inventory->slots[p->slot]);

    // take half of the items from the previous slot iff it has at least 2 items
    if(p->modifiers & kSplitStack && source.count >= 2) {
        // if destination is occupied, it must be the same type
        if(this->owner->inventory->isSlotOccupied(dstSlot) && 
           std::holds_alternative<Manager::InventoryBlock>(this->owner->inventory->slots[dstSlot])) {
            auto &dst = std::get<Manager::InventoryBlock>(this->owner->inventory->slots[dstSlot]);
            if(dst.blockId != source.blockId) return;

            const size_t toTake = std::min(source.count / 2, Manager::kMaxItemsPerSlot - dst.count);

            dst.count += toTake;
            source.count -= toTake;
        } 
        // otherwise, create a new slot there for the half items
        else {
            const size_t toTake = source.count / 2;
            source.count -= toTake;

            Manager::InventoryBlock b;
            b.count = toTake;
            b.blockId = source.blockId;

            this->owner->inventory->slots[dstSlot] = b;
        }
        goto beach;
    }
    // no flags; just swap the slots (or coalesce)
    else {
        // check if destination is of same type; if so, coalesce
        if(this->owner->inventory->isSlotOccupied(dstSlot) &&
           std::holds_alternative<Manager::InventoryBlock>(this->owner->inventory->slots[dstSlot])) {
            auto &dst = std::get<Manager::InventoryBlock>(this->owner->inventory->slots[dstSlot]);
            if(dst.blockId == source.blockId) {
                const size_t toTake = std::min(source.count, Manager::kMaxItemsPerSlot - dst.count);

                dst.count += toTake;
                source.count -= toTake;

                goto beach;
            } else {
                goto swap;
            }
        }

swap:;
        const auto temp = this->owner->inventory->slots[p->slot];
        this->owner->inventory->slots[p->slot] = this->owner->inventory->slots[dstSlot];
        this->owner->inventory->slots[dstSlot] = temp;
        goto beach;
    }

beach:;
    // finished, clean up empty slots
    this->owner->inventory->removeEmptySlots();
    this->owner->inventory->markDirty();
}

/**
 * Handles an accepted drop of a block from the registry. This will drop a full stack of items in
 * an empty slot, or top up the stack under the items to max level, if it's the same block type.
 */
void UIDetail::handleItemDrop(const size_t slotIdx, const RegisteredBlockDragPayload *p) {
    LOCK_GUARD(this->owner->inventory->slotLock, HandleSlotDrop);

    // handle the case where that slot is empty
    if(!this->owner->inventory->isSlotOccupied(slotIdx)) {
        Manager::InventoryBlock b;
        b.count = Manager::kMaxItemsPerSlot;
        b.blockId = p->blockId;

        this->owner->inventory->slots[slotIdx] = b;
    }
    // check the slot
    else {
        if(!std::holds_alternative<Manager::InventoryBlock>(this->owner->inventory->slots[slotIdx])) return;
        auto &destination = std::get<Manager::InventoryBlock>(this->owner->inventory->slots[slotIdx]);

        if(destination.blockId != p->blockId) return;

        destination.count = Manager::kMaxItemsPerSlot;
    }
}

/**
 * Draws the delete item. It accepts drops from all inventory slots and allows them to be emptied.
 */
void UIDetail::drawDeleteItem(const glm::vec2 &origin, gui::GameUI *gui) {
    // draw the item and its icon
    ItemDrawing::drawItemBackground(origin);

    const auto uvs = world::BlockRegistry::getTextureUv(this->deleteSlotTex);
    const auto uv0 = glm::vec2(uvs.x, uvs.y), uv1 = glm::vec2(uvs.z, uvs.w);
    const auto texId = (ImTextureID) (size_t) this->owner->atlas->getGlObjectId();
    const auto iOrg = origin + glm::vec2(1);

    auto d = ImGui::GetWindowDrawList();
    d->AddImage(texId, iOrg, iOrg+glm::vec2(48, 48), uv0, uv1);

    // drag and drop stuff
    ImGui::Dummy(glm::vec2(ItemDrawing::kItemSize));

    if(ImGui::BeginDragDropTarget()) {
        // accept inventory slot data
        if(const auto payload = ImGui::AcceptDragDropPayload(kInventorySlotDragType)) {
            XASSERT(payload->DataSize == sizeof(SlotDragPayload), "Invalid paylad size {}", payload->DataSize);

            const auto fromSlot = reinterpret_cast<SlotDragPayload *>(payload->Data);

            LOCK_GUARD(this->owner->inventory->slotLock, DeleteSlot);
            if(fromSlot->modifiers & kSplitStack) {
                auto &source = std::get<Manager::InventoryBlock>(this->owner->inventory->slots[fromSlot->slot]);
                source.count /= 2;
            } else {
                this->owner->inventory->slots[fromSlot->slot] = std::monostate();
            }

            this->owner->inventory->markDirty();
        }

        // end drop target
        ImGui::EndDragDropTarget();
    }
}



/**
 * Displays the panel that lists all registered items.
 */
void UIDetail::displayRegisteredItemsWindow(gui::GameUI *ui) {
    if(ImGui::BeginTabBar("Registered Items", 0)) {
        // blocks
        if(ImGui::BeginTabItem("Blocks")) {
            this->drawRegisteredBlocksTable(ui);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

/**
 * Draws the table listing all registered block types.
 */
void UIDetail::drawRegisteredBlocksTable(gui::GameUI *ui) {
    // begin table
    ImVec2 outerSize(0, -1);
    if(!ImGui::BeginTable("Blocks", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ColumnsWidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    // header
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34);
    ImGui::TableSetupColumn("Name");
    ImGui::TableHeadersRow();

    // registered blocks
    world::BlockRegistry::iterateBlocks([&](const auto &uuid, const auto block) {
        const auto uuidStr = uuids::to_string(uuid);

        ImGui::TableNextRow();
        ImGui::PushID(uuidStr.c_str());

        // icon
        ImGui::TableNextColumn();
        ItemDrawing::drawBlockIcon(ImGui::GetCursorScreenPos(), uuid, glm::vec2(38.), false);

        // dragging support
        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            RegisteredBlockDragPayload p;
            p.blockId = uuid;

            ImGui::SetDragDropPayload(kRegisteredBlockDragType, &p, sizeof(p));

            this->dragTooltipForItem(p);
            ImGui::EndDragDropSource();
        }

        // name
        ImGui::TableNextColumn();
        ImGui::Text("%s\n%s", block->getInternalName().c_str(), uuidStr.c_str());

        ImGui::PopID();
    });

    // done
    ImGui::EndTable();
}

