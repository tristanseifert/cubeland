/**
 * At the top of the screen will be rendered a global game menu bar. Its visibility can be adjusted
 * by calling into the handler routines.
 *
 * Additionally, debuggers may register themselves to show in the various debug menus.
 */
#ifndef GUI_MENUBARHANDLER_H
#define GUI_MENUBARHANDLER_H

#include "RunLoopStep.h"

#include <string>
#include <map>
#include <unordered_map>

namespace gui {
class MainWindow;

class MenuBarHandler: public RunLoopStep {
    friend class MainWindow;

    public:
        static bool isVisible() {
            return gShared->showMenuBar;
        }

        static void setVisible(const bool visible) {
            gShared->showMenuBar = visible;
        }

        static uint32_t registerItem(const std::string &category, const std::string &title, bool *value = nullptr) {
            return gShared->add(category, title, value);
        }
        static void unregisterItem(const uint32_t token) {
            if(!gShared) return;
            gShared->remove(token);
        }

    public:
        MenuBarHandler();
        ~MenuBarHandler() {
            gShared = nullptr;
        }

        virtual void draw() override;
        virtual bool handleEvent(const SDL_Event &) override {
            return false;
        }

    private:
        uint32_t add(const std::string &category, const std::string &title, bool *value);
        void remove(const uint32_t token);

    private:
        /// Item in a menu, as registered
        struct MenuItem {
            std::string title;
            bool *value = nullptr;
        };

        /// Menu containing submenu items
        struct Menu {
            std::string title;
            std::map<uint32_t, MenuItem> items;
        };

    private:
        void drawMenu(const Menu &);

    private:
        static MenuBarHandler *gShared;

    private:
        bool showMenuBar = false;

        /// each menu item gets a token so it can be de-registered later
        uint32_t nextToken = 1;

        /// registered menus
        std::unordered_map<std::string, Menu> menus;
};
}

#endif
