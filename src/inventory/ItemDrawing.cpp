#include "ItemDrawing.h"

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"
#include "gfx/gl/texture/Texture2D.h"

#include <imgui.h>

using namespace inventory;

ImFont *ItemDrawing::countFont = nullptr;
gfx::Texture2D *ItemDrawing::atlas = nullptr;


/**
 * Draws the background for a slot.
 */
void ItemDrawing::drawItemBackground(const glm::vec2 &origin, const bool selected) {
    auto d = ImGui::GetWindowDrawList();
    d->AddRect(origin, origin + glm::vec2(kItemSize), ImGui::GetColorU32(kBorderColor));

    if(selected) {
        d->AddRectFilled(origin + glm::vec2(1), origin + glm::vec2(kItemSize-1),
                ImGui::GetColorU32(kSelectionBackground));
    }
}

/**
 * Draws a stack of blocks item.
 *
 * @param count Number to display on the stack; pass 0 to not draw a number.
 */
void ItemDrawing::drawBlockItem(const glm::vec2 &origin, const uuids::uuid &blockId, const size_t count) {
    // draw icon and count
    auto d = ImGui::GetWindowDrawList();

    const auto iOrg = origin + glm::vec2(1);
    drawBlockIcon(iOrg, blockId, glm::vec2(48.));

    if(count) {
        char str[9];
        memset(&str, 0, sizeof(str));

        snprintf(str, 8, "%zu", count);

        // then draw the text
        const auto textOrg = iOrg + glm::vec2(4, 30);
        d->AddText(countFont, 17., textOrg, ImGui::GetColorU32(kCountColor), str);
    }
}

/**
 * Draws the icon for a block. It is inserted in the current window's draw list at the specified
 * position.
 */
void ItemDrawing::drawBlockIcon(const glm::vec2 &origin, const uuids::uuid &blockId, const glm::vec2 &size, bool direct) {
    glm::vec2 uv0(0), uv1(1);
    ImTextureID texId = nullptr;

    // get the texture for this block
    auto blockPtr = world::BlockRegistry::getBlock(blockId);
    if(blockPtr) {
        const auto previewTexId = blockPtr->getInventoryIcon();
        const auto uvs = world::BlockRegistry::getTextureUv(previewTexId);

        uv0 = glm::vec2(uvs.x, uvs.y);
        uv1 = glm::vec2(uvs.z, uvs.w);

        texId = (ImTextureID) (size_t) atlas->getGlObjectId();
    }

    // draw icon
    if(direct) {
        auto d = ImGui::GetWindowDrawList();

        if(texId) {
            d->AddImage(texId, origin, origin+size, uv0, uv1);
        } else {
            // TODO: draw a placeholder
        }
    } else {
        if(texId) {
            ImGui::Image(texId, size, uv0, uv1);
        } else {
            // TODO: draw a placeholder
        }
    }
}

