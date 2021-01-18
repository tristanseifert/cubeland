#include "InGamePrefsWindow.h"

#include "render/WorldRenderer.h"
#include "io/PrefsManager.h"

#include <imgui.h>

using namespace gui;
using namespace io;


/**
 * Loads preferences.
 */
void InGamePrefsWindow::load() {
    this->fov = PrefsManager::getFloat("gfx.fov", 74.);
    this->renderDist = PrefsManager::getUnsigned("world.render.distance", 2);
    this->inventoryHoriz = PrefsManager::getBool("ui.inventory.isHorizontal", true);
    this->vsync = PrefsManager::getBool("window.vsync", true);
}

/**
 * Saves preferences.
 */
void InGamePrefsWindow::save() {
    PrefsManager::setFloat("gfx.fov", this->fov);
    PrefsManager::setUnsigned("world.render.distance", std::max(1, this->renderDist));
    PrefsManager::setBool("ui.inventory.isHorizontal", this->inventoryHoriz);
    PrefsManager::setBool("window.vsync", this->vsync);

    // update world renderer
    this->renderer->requestPrefsLoad();
}

/**
 * Draws the in-game preferences window.
 */
void InGamePrefsWindow::draw(GameUI *gui) {
    bool dirty = false;

    // constrain prefs window size
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    ImGui::SetNextWindowFocus();
    ImGui::SetNextWindowSize(ImVec2(450, 0));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    // short circuit drawing if not visible
    if(!ImGui::Begin("Preferences", &this->visible, winFlags)) {
        return ImGui::End();
    }

    ImGui::PushItemWidth(250.);

    // chunk drawing related stuff
    if(ImGui::SliderFloat("Field of View", &this->fov, 25, 125, "%.1f", ImGuiSliderFlags_AlwaysClamp)) dirty = true;
    if(ImGui::SliderInt("Render Distance", &this->renderDist, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp)) dirty = true;

    // general UI
    ImGui::Dummy(ImVec2(0,2));
    if(ImGui::Checkbox("Enable VSync", &this->vsync)) dirty = true;

    // inventory
    ImGui::Dummy(ImVec2(0,2));
    if(ImGui::Checkbox("Horizontal Inventory Bar", &this->inventoryHoriz)) dirty = true;

    // finish drawing
    ImGui::PopItemWidth();

    ImGui::Separator();
    ImGui::TextWrapped("%s", "Options not shown here cannot be changed while in game. Exit to the main screen to change them.");

    ImGui::End();

    // save if needed
    if(dirty) {
        this->save();
    }
}
