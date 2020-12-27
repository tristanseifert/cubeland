/**
 * Draws both the inventory bar at the bottom of the screen, and the inventory reorganization
 * window.
 */
#ifndef INVENTORY_INVENTORYUI_H
#define INVENTORY_INVENTORYUI_H

#include "gui/GameWindow.h"

namespace gfx {
class Texture2D;
}

namespace inventory {
class Manager;

class UIBar;
class UIDetail;

class UI: public gui::GameWindow {
    friend class UIBar;
    friend class UIDetail;

    public:
        UI(Manager *invMgr);
        ~UI();

        void draw(gui::GameUI *gui) override;

        /// Inventory bar is always visible
        bool isVisible() const override {
            return true;
        }

        /// whether the inventory detail is open or not
        bool isDetailOpen() const {
            return this->showsDetail;
        }
        /// sets whether the detailed inventory management window is open
        void setDetailOpen(const bool val) {
            this->showsDetail = val;
            if(!val) {
                this->shouldClose = true;
            }
        }

    private:
        void uploadAtlasTexture();

    private:
        Manager *inventory = nullptr;

        UIBar *bar = nullptr;
        UIDetail *detail = nullptr;

        bool shouldClose = false;
        bool showsDetail = false;

        /// texture atlas for inventory images
        gfx::Texture2D *atlas = nullptr;
};
}

#endif
