#ifndef INVENTORY_UIDETAIL_H
#define INVENTORY_UIDETAIL_H

#include "world/block/BlockRegistry.h"

#include <cstddef>

#include <uuid.h>
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
        UIDetail(UI *owner);

        void draw(gui::GameUI *gui);

        /// Load user preferences; current no-op
        void loadPrefs() {}

    private:
        /// drag payload type for slots in inventory
        constexpr static const char *kInventorySlotDragType = "InventoryCell";
        /// drag payload type for blocks dragged from the registered blocks list
        constexpr static const char *kRegisteredBlockDragType = "RegisteredBlock";

        /// width of the registered items section
        constexpr static const float kRegisteredItemsWidth = 335.;

    private:
        /// modifiers in inventory slot dragging
        enum SlotDragModifiers {
            kNoModifiers                        = 0,
            kSplitStack
        };

        struct DragPayload {
            SlotDragModifiers modifiers = kNoModifiers;
        };

        /// payload of drags of inventory slots
        struct SlotDragPayload: public DragPayload {
            size_t slot;
        };

        /// payload of drags from registered blocks pane
        struct RegisteredBlockDragPayload: public DragPayload {
            uuids::uuid blockId;
        };

    private:
        void drawRow(gui::GameUI *gui, const size_t offset);
        void drawDeleteItem(const glm::vec2&, gui::GameUI *);

        void dragTooltipForItem(const SlotDragPayload &);
        void handleItemDrop(const size_t, const SlotDragPayload *);

        void dragTooltipForItem(const RegisteredBlockDragPayload &);
        void handleItemDrop(const size_t, const RegisteredBlockDragPayload *);

        void displayRegisteredItemsWindow(gui::GameUI *);
        void drawRegisteredBlocksTable(gui::GameUI *);

    private:
        UI *owner = nullptr;

        /// should the list of registered blocks/items be shown?
        bool showsRegisteredItems = true;

        /// texture ID for the delete slot
        world::BlockRegistry::TextureId deleteSlotTex = 0;
};
}

#endif
