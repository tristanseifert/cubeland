#include "MenuBarHandler.h"

#include <imgui.h>
#include <mutils/time/profiler.h>

#include <algorithm>
#include <stdexcept>

using namespace gui;

MenuBarHandler *MenuBarHandler::gShared = nullptr;

/**
 * Sets the shared instance pointer on first allocation.
 */
MenuBarHandler::MenuBarHandler() {
    if(!gShared) {
        gShared = this;
    }
}



/**
 * Adds a new menu item.
 */
uint32_t MenuBarHandler::add(const std::string &category, const std::string &title, bool *value) {
    // create the category if needed
    if(!this->menus.contains(category)) {
        Menu m;
        m.title = title;
        this->menus[category] = m;
    }

    auto &menu = this->menus[category];

    // add the item
    const uint32_t token = this->nextToken++;
    MenuItem item;
    item.title = title;
    item.value = value;

    menu.items[token] = item;

    return token;
}

/**
 * Removes an existing menu item.
 */
void MenuBarHandler::remove(const uint32_t token) {
    for(auto &[category, menu] : this->menus) {
        if(menu.items.contains(token)) {
            menu.items.erase(token);
            goto beach;
        }
    }

    throw std::runtime_error("Attempt to deregister unknown menu item");

beach:;
    // remove any menus that became empty
    std::erase_if(this->menus, [](const auto &item) {
        const auto &[category, menu] = item;
        return menu.items.empty();
    });
}





/**
 * Draws the main menu bar.
 */
void MenuBarHandler::draw() {
    if(!this->showMenuBar) return;

    PROFILE_SCOPE(MenuBar);
    if(!ImGui::BeginMainMenuBar()) {
        return;
    }

    // draw all menus
    for(const auto &[title, menu] : this->menus) {
        if(ImGui::BeginMenu(title.c_str())) {
            this->drawMenu(menu);
            ImGui::EndMenu();
        }
    }

    // clean up
    ImGui::EndMainMenuBar();
}

/**
 * Draws all menu items in this menu, including sub-items if needed.
 */
void MenuBarHandler::drawMenu(const Menu &menu) {
    // items
    for(const auto &[token, item] : menu.items) {
        if(item.value) {
            ImGui::MenuItem(item.title.c_str(), nullptr, item.value);
        } else {
            ImGui::MenuItem(item.title.c_str());
        }
    }
}

