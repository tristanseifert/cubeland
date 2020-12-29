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

    private:
        UI *owner = nullptr;

        bool showsOverlay = true;

        /// when set, the items are shown horizontally; otherwise, they're vertical
        bool isHorizontal = false;
};
}

#endif
