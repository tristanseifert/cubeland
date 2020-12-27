#include "UIDetail.h"
#include "UI.h"
#include "Manager.h"

#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"
#include "gui/GameUI.h"
#include "gfx/gl/texture/Texture2D.h"

#include <mutils/time/profiler.h>
#include <Logging.h>

#include <imgui.h>

#include <algorithm>

using namespace inventory;

/**
 * Draws the main window of the detail view.
 *
 * Note that this is called from within BeginPopupModal() context already, so there is no need to
 * create (or end) the window to hold the view.
 */
void UIDetail::draw(gui::GameUI *ui) {
    PROFILE_SCOPE(InventoryWindowDraw);

    // get fonts if needed
    if(!this->countFont) {
        this->countFont = ui->getFont(gui::GameUI::kGameFontBold);
    }

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

/**
 * Draws a row of 10 items.
 */
void UIDetail::drawRow(gui::GameUI *gui, const size_t offset) {
    for(size_t i = offset; i < offset + 10; i++) {
        ImGui::PushID(i);

        glm::vec2 pos = ImGui::GetCursorScreenPos();
        const bool occupied = this->owner->inventory->isSlotOccupied(i);

        this->drawItemBackground(pos, i);

        if(occupied) {
            this->drawItem(pos, i);
        }

        // spacing
        ImGui::Dummy(glm::vec2(kItemSize, kItemSize));

        // drag source (if the slot is NOT empty)
        if(occupied) {
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

            // end drop target
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::PopID();
    }
}

/**
 * Draws the background for a slot.
 */
void UIDetail::drawItemBackground(const glm::vec2 &origin, const size_t slot) {
    auto d = ImGui::GetWindowDrawList();
    d->AddRect(origin, origin + glm::vec2(kItemSize), ImGui::GetColorU32(kBorderColor));
}

/**
 * Draws the item for an occupied inventory slot.
 */
void UIDetail::drawItem(const glm::vec2 &origin, const size_t slotIdx) {
    const auto &slot = this->owner->inventory->slots[slotIdx];

    size_t count = 0;
    glm::vec2 uv0(0), uv1(1);
    ImTextureID texId = nullptr;

    // does the slot contain a block?
    if(std::holds_alternative<Manager::InventoryBlock>(slot)) {
        const auto &block = std::get<Manager::InventoryBlock>(slot);
        count = block.count;

        // get the texture for this block
        auto blockPtr = world::BlockRegistry::getBlock(block.blockId);
        if(blockPtr) {
            const auto previewTexId = blockPtr->getInventoryIcon();
            const auto uvs = world::BlockRegistry::getTextureUv(previewTexId);

            uv0 = glm::vec2(uvs.x, uvs.y);
            uv1 = glm::vec2(uvs.z, uvs.w);

            texId = (ImTextureID) (size_t) this->owner->atlas->getGlObjectId();
        }
    }

    // draw icon and count
    auto d = ImGui::GetWindowDrawList();

    const auto iOrg = origin + glm::vec2(1);
    if(texId) {
        d->AddImage(texId, iOrg, iOrg+glm::vec2(48, 48), uv0, uv1);
    } else {
        // TODO: draw a placeholder
    }

    if(count) {
        char str[9];
        memset(&str, 0, sizeof(str));

        snprintf(str, 8, "%zu", count);

        // then draw the text
        const auto textOrg = iOrg + glm::vec2(4, 30);
        d->AddText(this->countFont, 17., textOrg, ImGui::GetColorU32(kCountColor), str);
    }
}

/**
 * Draws a drag tooltip for a given item. This contains the image, type, and count.
 */
void UIDetail::dragTooltipForItem(const SlotDragPayload &payload) {
    const auto &slot = this->owner->inventory->slots[payload.slot];

    size_t count = 0;
    glm::vec2 uv0(0), uv1(1);
    ImTextureID texId = nullptr;

    std::string name;

    // does the slot contain a block?
    if(std::holds_alternative<Manager::InventoryBlock>(slot)) {
        const auto &block = std::get<Manager::InventoryBlock>(slot);
        auto bo = world::BlockRegistry::getBlock(block.blockId);

        count = block.count;
        name = bo->getInternalName();

        // get the texture for this block
        const auto previewTexId = bo->getInventoryIcon();
        if(previewTexId) {
            const auto uvs = world::BlockRegistry::getTextureUv(previewTexId);

            uv0 = glm::vec2(uvs.x, uvs.y);
            uv1 = glm::vec2(uvs.z, uvs.w);

            texId = (ImTextureID) (size_t) this->owner->atlas->getGlObjectId();
        }
    }

    // image
    ImGui::Image(texId, glm::vec2(kItemSize - 2), uv0, uv1);
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
 * Handles an accepted drop of an inventory slot.
 */
void UIDetail::handleItemDrop(const size_t dstSlot, const SlotDragPayload *p) {
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
}
