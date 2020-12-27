/**
 * Draws the inventory bar at the bottom of the screen.
 */
#ifndef INVENTORY_UIBAR_H
#define INVENTORY_UIBAR_H

#include <cstddef>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct ImFont;

namespace gui {
class GameUI;
}

namespace inventory {
class UI;

class UIBar {
    public:
        UIBar(UI *owner);

        bool draw(gui::GameUI *gui, bool end = true);

    private:
        /// Padding between display edge and inventory window
        constexpr static const float kEdgePadding = 5.;
        /// Alpha value for rendering the inventory overlay
        constexpr static const float kOverlayAlpha = .85;

        /// square size of an item
        constexpr static const float kItemSize = 50;

        /// color of the item borders
        constexpr static const glm::vec4 kBorderColor = glm::vec4(0.33, 0.33, 0.33, 1);
        /// fill color for the selection indicator
        constexpr static const glm::vec4 kSelectionBackground = glm::vec4(1, 1, 0, 0.66);

        /// background color for the item stack count
        constexpr static const glm::vec4 kCountBackground = glm::vec4(0, 0, 0, 0.33);
        /// Text color for the item stack count
        constexpr static const glm::vec4 kCountColor = glm::vec4(1, 1, 1, 1);

    private:
        void drawItemBackground(const glm::vec2 &, const size_t);
        void drawItem(const glm::vec2 &, const size_t);

    private:
        UI *owner = nullptr;

        bool showsOverlay = true;

        /// when set, the items are shown horizontally; otherwise, they're vertical
        bool isHorizontal = true;

        /// font for displaying the count of items
        ImFont *countFont = nullptr;
};
}

#endif
