#ifndef INVENTORY_UIDETAIL_H
#define INVENTORY_UIDETAIL_H

#include <cstddef>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct ImFont;

namespace gui {
class GameUI;
}

namespace inventory {
class UI;

class UIDetail {
    public:
        UIDetail(UI *_owner) : owner(_owner) {};

        void draw(gui::GameUI *gui);

    private:
        /// square size of an item
        constexpr static const float kItemSize = 50;

        /// color of the item borders
        constexpr static const glm::vec4 kBorderColor = glm::vec4(0.33, 0.33, 0.33, 1);
        /// Text color for the item stack count
        constexpr static const glm::vec4 kCountColor = glm::vec4(1, 1, 1, 1);

        /// drag payload type for slots in inventory
        constexpr static const char *kInventorySlotDragType = "InventoryCell";

    private:
        /// modifiers in inventory slot dragging
        enum SlotDragModifiers {
            kNoModifiers                        = 0,
            kSplitStack
        };

        /// payload of drags of inventory slots
        struct SlotDragPayload {
            size_t slot;
            SlotDragModifiers modifiers = kNoModifiers;
        };

    private:
        void drawRow(gui::GameUI *gui, const size_t offset);
        void drawItemBackground(const glm::vec2 &, const size_t);
        void drawItem(const glm::vec2 &, const size_t);

        void dragTooltipForItem(const SlotDragPayload &);

        void handleItemDrop(const size_t, const SlotDragPayload *);

    private:
        UI *owner = nullptr;

        /// font for displaying the count of items
        ImFont *countFont = nullptr;
};
}

#endif
