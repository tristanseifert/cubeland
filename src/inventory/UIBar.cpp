#include "UIBar.h"
#include "UI.h"
#include "ItemDrawing.h"
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

        ItemDrawing::drawItemBackground(pos, (this->owner->inventory->getSelectedSlot() == i));

        if(this->owner->inventory->isSlotOccupied(i)) {
            this->owner->drawItem(pos, i);
        }

        // spacing
        ImGui::Dummy(glm::vec2(ItemDrawing::kItemSize));
        if(this->isHorizontal) {
            ImGui::SameLine();
        }
    }

    // finish draw
    return true;
}

