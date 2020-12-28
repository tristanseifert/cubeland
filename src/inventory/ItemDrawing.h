/**
 * Provides helper functions for drawing items in the user interface. This is responsible for
 * displaying things in the inventory UI, for example.
 */
#ifndef INVENTORY_ITEMDRAWING_H
#define INVENTORY_ITEMDRAWING_H

#include <cstddef>

#include <uuid.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct ImFont;

namespace gfx {
class Texture2D;
}

namespace inventory {
class ItemDrawing {
    public:
        /// square size of an item
        constexpr static const float kItemSize = 50;

    public:
        /// Sets the font used for displaying counts on icons.
        static void setCountFont(ImFont *newFont) {
            countFont = newFont;
        }
        /// Sets the texture containing inventory icons.
        static void setAtlasTexture(gfx::Texture2D *texture) {
            atlas = texture;
        }

        static void drawItemBackground(const glm::vec2 &origin, const bool selected = false);

        static void drawBlockItem(const glm::vec2 &origin, const uuids::uuid &blockId, const size_t count = 0);
        static void drawBlockIcon(const glm::vec2 &origin, const uuids::uuid &blockId, const glm::vec2 &size = glm::vec2(48.), const bool direct = true);

    private:
        /// color of the item borders
        constexpr static const glm::vec4 kBorderColor = glm::vec4(0.33, 0.33, 0.33, 1);
        /// fill color for the selection indicator
        constexpr static const glm::vec4 kSelectionBackground = glm::vec4(1, 1, 0, 0.66);
        /// Text color for the item stack count
        constexpr static const glm::vec4 kCountColor = glm::vec4(1, 1, 1, 1);

    private:
        /// Texture containing all item icons
        static gfx::Texture2D *atlas;
        /// Font for rendering the item counts
        static ImFont *countFont;
};
}

#endif
