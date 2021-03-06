#include "PreferencesWindow.h"
#include "GameUI.h"
#include "MainWindow.h"
#include "io/PrefsManager.h"
#include "io/Format.h"

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <imgui.h>

#include <thread>

using namespace gl;
using namespace gui;

/**
 * Sets up the UI with the state of the preferences.
 */
PreferencesWindow::PreferencesWindow(MainWindow *_window) : window(_window) {
    this->load();
}

/**
 * Draws the prefs window.
 *
 * It has several tabbed sections.
 */
void PreferencesWindow::draw(GameUI *ui) {
    // constrain prefs window size
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    // ImGui::SetNextWindowFocus();
    ImGui::SetNextWindowSize(ImVec2(800, 600));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    // short circuit drawing if not visible
    if(!ImGui::Begin("Preferences", &this->visible, winFlags)) {
        return ImGui::End();
    }

    // tab bar (for each section)
    if(ImGui::BeginTabBar("head")) {
        if(ImGui::BeginTabItem("User Interface")) {
            this->drawUiPane(ui);
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Graphics")) {
            this->drawGfxPane(ui);
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Performance")) {
            this->drawPerfPane(ui);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

/**
 * Reads the user interface preferences.
 */
void PreferencesWindow::loadUiPaneState() {
    this->stateUi.restoreWindowSize = io::PrefsManager::getBool("window.restoreSize", true);
    this->stateUi.dpiAware = io::PrefsManager::getBool("window.hiDpi", false);
    this->stateUi.vsync = io::PrefsManager::getBool("window.vsync", true);
}
/**
 * Writes the settings displayed on the UI preferences pane back to the preferences.
 */
void PreferencesWindow::saveUiPaneState() {
    io::PrefsManager::setBool("window.restoreSize", this->stateUi.restoreWindowSize);
    io::PrefsManager::setBool("window.hiDpi", this->stateUi.dpiAware);
    io::PrefsManager::setBool("window.vsync", this->stateUi.vsync);
}
/**
 * Draws the "User Interface" preferences pane.
 */
void PreferencesWindow::drawUiPane(GameUI *ui) {
    bool dirty = false;

    // current UI driver
    this->drawKeyValue(ui, "Window driver", "SDL/OpenGL");
    this->drawKeyValue(ui, "GL driver", f("{} ({})", glGetString(GL_RENDERER), glGetString(GL_VERSION)));

    // restore window size checkbox
    if(ImGui::Checkbox("Restore window size", &this->stateUi.restoreWindowSize)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When set, the main window's dimensions are persisted across app launches.");
    }

    // DPI awareness
    if(ImGui::Checkbox("HiDPI Aware", &this->stateUi.dpiAware)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Request a HiDPI rendering context, resulting in much crisper output on scaled displays, at the cost of performance.\nNote: You must restart the app for this setting to take effect.");
    }

    // VSync
    if(ImGui::Checkbox("Enable VSync", &this->stateUi.vsync)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Synchronize drawing with the display's refresh interval, effectively setting the maximum frame rate to the display refresh rate.\nNote: Disabling this option may result in visual artifacts.");
    }
    // save if needed
    if(dirty) {
        this->saveUiPaneState();
        this->window->loadPrefs();
    }
}



/**
 * Loads the graphics preferences.
 */
void PreferencesWindow::loadGfxPaneState() {
    this->gfx.fancySky = io::PrefsManager::getBool("gfx.fancySky", true);
    this->gfx.dirShadows = io::PrefsManager::getBool("gfx.sunShadow");
    this->gfx.ssao = io::PrefsManager::getBool("gfx.ssao", true);
    this->gfx.gamma = io::PrefsManager::getFloat("gfx.fxaa.gamma", 2.2);
    this->gfx.fov = io::PrefsManager::getFloat("gfx.fov", 74.);
    this->gfx.horizontalInventory = io::PrefsManager::getBool("ui.inventory.isHorizontal", true);
}
/**
 * Saves the graphics preferences.
 */
void PreferencesWindow::saveGfxPaneState() {
    io::PrefsManager::setBool("gfx.fancySky", this->gfx.fancySky);
    io::PrefsManager::setBool("gfx.sunShadow", this->gfx.dirShadows);
    io::PrefsManager::setBool("gfx.ssao", this->gfx.ssao);
    io::PrefsManager::setFloat("gfx.fxaa.gamma", this->gfx.gamma);
    io::PrefsManager::setFloat("gfx.fov", this->gfx.fov);
    io::PrefsManager::setBool("ui.inventory.isHorizontal", this->gfx.horizontalInventory);
}
/**
 * Draws the "graphics" preferences pane.
 */
void PreferencesWindow::drawGfxPane(GameUI *ui) {
    bool dirty = false;

    // actions (for loading preset)
    constexpr static const size_t kNumPresets = 4;
    static const char *kPresetNames[kNumPresets] = {
        "Low", "Medium", "High", "Make my GPU hurt"
    };

    if(ImGui::Button("Load Preset")) {
        (this->kGfxPresets[this->gfx.preset % 4])();
        dirty = true;
    } if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Replaces graphics settings with the selected preset.");
    }

    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    if(ImGui::BeginCombo("Preset", kPresetNames[this->gfx.preset])) {
        for(size_t j = 0; j < kNumPresets-1; j++) {
            const bool isSelected = (this->gfx.preset == j);

            if(ImGui::Selectable(kPresetNames[j], isSelected)) {
                this->gfx.preset = j;
            }
            if(isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::Separator();

    // left column: graphics steps / rendering
    const float w = (ImGui::GetContentRegionAvail().x / 2.) - 4;
    if(ImGui::BeginChild("##gfx", ImVec2(w, 0), false, ImGuiWindowFlags_NoBackground)) {
        // whether fancy sky is used
        if(ImGui::Checkbox("Fancy Sky", &this->gfx.fancySky)) dirty = true;
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Draws physically accurate clouds and sun using shaders.");
        }

        // shadows
        if(ImGui::Checkbox("Directional Light Shadows", &this->gfx.dirShadows)) dirty = true;
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Global light sources (e.g. sun and moon) will cast shadows when enabled.");
        }

        // ambient occlusion
        if(ImGui::Checkbox("Ambient Occlusion", &this->gfx.ssao)) dirty = true;
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Selectively darkens areas of intersecting planes, such as corners of rooms.");
        }
    }
    ImGui::EndChild();

    // right column: cosmetic
    ImGui::SameLine();
    if(ImGui::BeginChild("##cosmetic", ImVec2(w, 0), false, ImGuiWindowFlags_NoBackground)) {
        // field of view
        if(ImGui::SliderFloat("Field of View", &this->gfx.fov, 25, 125, "%.1f", ImGuiSliderFlags_AlwaysClamp)) dirty = true;
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts how much of the environment is visible.");
        }

        // gamma
        if(ImGui::SliderFloat("Gamma", &this->gfx.gamma, .5, 5., "%.1f", ImGuiSliderFlags_AlwaysClamp)) dirty = true;
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls the brightness of the game content.\nStandard values range from 1.8 - 2.2.");
        }

        // inventory orientation
        ImGui::Dummy(ImVec2(0, 2));
        if(ImGui::Checkbox("Horizontal Inventory Bar", &this->gfx.horizontalInventory)) dirty = true;
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("The inventory bar displays the first ten items in your inventory.\nThese are the items you can place/use without opening the full inventory.");
        }
    }
    ImGui::EndChild();


    // save if needed
    if(dirty) {
        this->saveGfxPaneState();
    }
}



/**
 * Loads preferences for the performance pane.
 */
void PreferencesWindow::loadPerfPaneState() {
    this->perf.drawThreads = io::PrefsManager::getUnsigned("chunk.drawWorkThreads", 4);
    this->perf.sourceThreads = io::PrefsManager::getUnsigned("world.sourceWorkThreads", 2);
    this->perf.renderDist = io::PrefsManager::getUnsigned("world.render.distance", 2);
    this->perf.renderCacheBuffer = io::PrefsManager::getUnsigned("world.render.cacheRange", 1);
}

/**
 * Saves preferences for the performance pane.
 */
void PreferencesWindow::savePerfPaneState() {
    io::PrefsManager::setUnsigned("chunk.drawWorkThreads", std::max(2, this->perf.drawThreads));
    io::PrefsManager::setUnsigned("world.sourceWorkThreads", std::max(2, this->perf.sourceThreads));
    io::PrefsManager::setUnsigned("world.render.distance", std::max(1, this->perf.renderDist));
    io::PrefsManager::setUnsigned("world.render.cacheRange", std::max(1, this->perf.renderCacheBuffer));
}

/**
 * Draws the performance settings pane.
 */
void PreferencesWindow::drawPerfPane(GameUI *gui) {
    bool dirty = false;

    // detected CPU cores
    ImGui::Bullet();
    ImGui::Text("Available Processor Cores: %d", std::thread::hardware_concurrency());

    // vertex generator threads
    if(ImGui::InputInt("Drawing Threads", &this->perf.drawThreads)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drawing threads convert chunk data into on-screen vertices.\nHint: Increase this value to be approximately 2/3 the number of processor cores for optimal performance.");
    }

    // world source threads
    if(ImGui::InputInt("World Source Threads", &this->perf.sourceThreads)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("World source threads read world data and generates new chunks.\nHint: Multiplayer worlds may see performance gains from increasing this value.");
    }

    ImGui::Dummy(ImVec2(8, 0));
    // render distance
    if(ImGui::SliderInt("Render Distance", &this->perf.renderDist, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum number of chunks beyond the player's position to load. Each chunk is 256 blocks in each axis.");
    }
    // render cache
    if(ImGui::SliderInt("Render Cache Buffer", &this->perf.renderCacheBuffer, 1, 10, "%d", ImGuiSliderFlags_AlwaysClamp)) dirty = true;
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Added to the render distance to calculate the maximum distance a chunk can be from the player before it is evicted from caches.\nHint: Increase this value if your machine has plenty available RAM.");
    }
    // save if needed
    if(dirty) {
        this->savePerfPaneState();
    }
}



/**
 * Draws a key/value list item.
 */
void PreferencesWindow::drawKeyValue(GameUI *ui, const std::string &key, const std::string &value) {
    ImGui::PushFont(ui->getFont(GameUI::kGameFontBold));
    ImGui::Text("%s:", key.c_str());
    ImGui::SameLine();

    ImGui::PopFont();
    ImGui::Text("%s", value.c_str());
}

