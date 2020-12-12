#include "PreferencesWindow.h"
#include "GameUI.h"
#include "io/PrefsManager.h"
#include "io/Format.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

#include <imgui.h>

using namespace gl;
using namespace gui;

/**
 * Sets up the UI with the state of the preferences.
 */
PreferencesWindow::PreferencesWindow() {
    this->loadUiPaneState();
}

/**
 * Draws the prefs window.
 *
 * It has several tabbed sections.
 */
void PreferencesWindow::draw(GameUI *ui) {
    double spacing;

    // constrain prefs window size
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 480), ImVec2(640, 480));

    // short circuit drawing if not visible
    if(!ImGui::Begin("Preferences", &this->visible, ImGuiWindowFlags_NoResize)) {
        goto done;
    }

    // tab bar (for each section)
    if(ImGui::BeginTabBar("head")) {
        if(ImGui::BeginTabItem("User Interface")) {
            this->drawUiPane(ui);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // horizontal line and the close button
    ImGui::NewLine();
    spacing = ImGui::GetContentRegionAvail().y - (ImGui::GetFontSize() * 2);
    ImGui::Dummy(ImVec2(1, spacing));
    ImGui::Separator();

    if(ImGui::Button("Close")) {
        io::PrefsManager::synchronize();
        this->visible = false;
    }

done:;
    ImGui::End();
}

/**
 * Reads the user interface preferences.
 */
void PreferencesWindow::loadUiPaneState() {
    this->stateUi.restoreWindowSize = io::PrefsManager::getBool("window.restoreSize");
}
/**
 * Writes the settings displayed on the UI preferences pane back to the preferences.
 */
void PreferencesWindow::saveUiPaneState() {
    io::PrefsManager::setBool("window.restoreSize", this->stateUi.restoreWindowSize);
}
/**
 * Draws the "User Interface" preferences pane.
 */
void PreferencesWindow::drawUiPane(GameUI *ui) {
    // current UI driver
    this->drawKeyValue(ui, "Window driver", "SDL/OpenGL");
    this->drawKeyValue(ui, "GL driver", f("{} ({})", glGetString(GL_RENDERER), glGetString(GL_VERSION)));

    // restore window size checkbox
    if(ImGui::Checkbox("Restore window size", &this->stateUi.restoreWindowSize)) {
        this->saveUiPaneState();
    }
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When set, the main window's dimensions are persisted across app launches.");
    }

    // DPI awareness
    if(ImGui::Checkbox("HiDPI Aware", &this->stateUi.dpiAware)) {
        this->saveUiPaneState();
    }
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Allows the app to use HiDPI rendering contexts. This results in much crisper output on scaled displays, at the cost of performance.");
    }
}

/**
 * Draws a key/value list item.
 */
void PreferencesWindow::drawKeyValue(GameUI *ui, const std::string &key, const std::string &value) {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::PushFont(ui->getFont(GameUI::kBoldFontName));
    ImGui::Text("%s:", key.c_str());
    ImGui::SameLine();

    ImGui::PopFont();
    ImGui::Text("%s", value.c_str());
}

