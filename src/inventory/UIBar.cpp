#include "UIBar.h"
#include "UI.h"
#include "Manager.h"

#include "world/block/BlockRegistry.h"
#include "world/block/Block.h"
#include "gui/GameUI.h"
#include "gfx/gl/texture/Texture2D.h"

#include <imgui.h>
#include <mutils/time/profiler.h>
#include <glm/glm.hpp>

#include <cstring>
#include <cstdio>

using namespace inventory;

/**
 * Initializes the inventory bar.
 */
UIBar::UIBar(UI *_owner) : owner(_owner) {

}

/**
 * Draws the inventory bar. It is always pinned to the bottom of the screen, at the center.
 *
 * @return Whether the bar was drawn or not
 */
bool UIBar::draw(gui::GameUI *ui, bool end) {
    PROFILE_SCOPE(InventoryBarDraw);

    // get fonts if needed
    if(!this->countFont) {
        this->countFont = ui->getFont(gui::GameUI::kGameFontBold);
    }

    // always centered at bottom middle
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    glm::vec2 windowPos(io.DisplaySize.x / 2., io.DisplaySize.y - kEdgePadding);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, glm::vec2(.5, 1));
    ImGui::SetNextWindowBgAlpha(kOverlayAlpha);

    if(!ImGui::Begin("Inventory Overlay", &this->showsOverlay, window_flags)) {
        return false;
    }

    // draw each of the 10 inventory slots
    for(size_t i = 0; i < 10; i++) {
        glm::vec2 pos = ImGui::GetCursorScreenPos();

        this->drawItemBackground(pos, i);

        if(this->owner->inventory->isSlotOccupied(i)) {
            this->drawItem(pos, i);
        }

        // spacing
        ImGui::Dummy(glm::vec2(kItemSize, kItemSize));
        if(this->isHorizontal) {
            ImGui::SameLine();
        }
    }

    // finish draw
    return true;
}

/**
 * Draws the background for a slot.
 */
void UIBar::drawItemBackground(const glm::vec2 &origin, const size_t slot) {
    auto d = ImGui::GetWindowDrawList();
    d->AddRect(origin, origin + glm::vec2(kItemSize), ImGui::GetColorU32(kBorderColor));

    // selection indicator
    if(this->owner->inventory->getSelectedSlot() == slot) {
        d->AddRectFilled(origin + glm::vec2(1), origin + glm::vec2(kItemSize-1),
                ImGui::GetColorU32(kSelectionBackground));
    }
}

/**
 * Draws the item for an occupied inventory slot.
 */
void UIBar::drawItem(const glm::vec2 &origin, const size_t slotIdx) {
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

        // draw the background for the text
        /*const glm::vec2 textSz = ImGui::CalcTextSize(str);

        const auto bgOrg = iOrg + glm::vec2(2, 29);
        const auto bgMax = bgOrg + textSz + glm::vec2(2, 0);

        d->AddRectFilled(bgOrg, bgMax, ImGui::GetColorU32(kCountBackground), 1.);*/

        // then draw the text
        const auto textOrg = iOrg + glm::vec2(4, 30);
        d->AddText(this->countFont, 17., textOrg, ImGui::GetColorU32(kCountColor), str);
    }
}
