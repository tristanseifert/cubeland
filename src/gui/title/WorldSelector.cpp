#include "WorldSelector.h"
#include "gui/GameUI.h"

#include "io/PrefsManager.h"
#include "io/Format.h"
#include <Logging.h>

#include <imgui.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

#include <cereal/types/array.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/portable_binary.hpp>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <ctime>

using namespace gui::title;

const std::string WorldSelector::kPrefsKey = "ui.worldSelector.recents";

/**
 * Initializes a world selector.
 */
WorldSelector::WorldSelector() {

}


/**
 * Loads the recents list from preferences.
 */
void WorldSelector::loadRecents() {
    // get the raw recents data blob
    auto blob = io::PrefsManager::getBlob(kPrefsKey);
    if(!blob) return;

    // attempt decoding
    try {
        std::stringstream stream(std::string(blob->begin(), blob->end()));

        cereal::PortableBinaryInputArchive arc(stream);

        Recents r;
        arc(r);

        this->recents = std::move(r);
    } catch(std::exception &e) {
        Logging::error("Failed to deserialize world file recents list: {}", e.what());
    }
}

/**
 * Saves the recents list to user preferences.
 */
void WorldSelector::saveRecents() {
    // sort recents in newest to oldest order
    std::sort(std::begin(this->recents.recents), std::end(this->recents.recents), [](const auto &l, const auto &r) {
        const std::chrono::system_clock::time_point longAgo;
        return ((l ? l->lastOpened : longAgo) > (r ? r->lastOpened : longAgo));
    });

    // write recents data
    std::stringstream stream;

    cereal::PortableBinaryOutputArchive arc(stream);
    arc(this->recents);

    const auto str = stream.str();

    std::vector<char> blobData(str.begin(), str.end());
    io::PrefsManager::setBlob(kPrefsKey, blobData);
}



/**
 * Draws the world selector window.
 */
void WorldSelector::draw(GameUI *gui) {
    using namespace igfd;

    // constrain prefs window size
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    if(!this->isFileDialogOpen) {
        ImGui::SetNextWindowFocus();
    }
    ImGui::SetNextWindowSize(ImVec2(800, 600));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    // short circuit drawing if not visible
    if(!ImGui::Begin("Open Single Player World", &this->visible, winFlags)) {
        return ImGui::End();
    }

    // list of recent worlds
    this->drawRecentsList(gui);

    // actions
    ImGui::Separator();
    if(ImGui::Button("Open Other...")) {
        ImGuiFileDialog::Instance()->OpenModal("OpenWorld", "Choose World File", "v1 World{.world}", ".");
        this->isFileDialogOpen = true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Create New...")) {

    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    if(ImGui::Button("Open Selected") && this->selectedWorld >= 0) {
        const auto &entry = this->recents.recents[this->selectedWorld];
        if(entry) {
            // TODO: do stuff here
            this->openWorld(entry->path);
        }
    }

    // file dialogs
    ImGui::SetNextWindowSize(ImVec2(640, 420));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    if(ImGuiFileDialog::Instance()->FileDialog("OpenWorld")) {
        if(ImGuiFileDialog::Instance()->IsOk) {
            const auto path = ImGuiFileDialog::Instance()->GetFilePathName();
            this->openWorld(path);
        }

        ImGuiFileDialog::Instance()->CloseDialog("OpenWorld");
        this->isFileDialogOpen = false;
    }

    // end
    ImGui::End();
}

/**
 * Draws the recents table.
 */
void WorldSelector::drawRecentsList(GameUI *gui) {
    // placeholder if empty
    if(this->recents.empty()) {
        ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
        ImGui::TextUnformatted("No recent worlds available");
        ImGui::PopFont();

        return;
    }

    // otherwise, begin the table
    const auto tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_BordersOuter;
    const ImVec2 tableSize(-FLT_MIN, 520);

    if(ImGui::BeginTable("##recents", 1, tableFlags, tableSize)) {
        ImGui::TableSetupColumn("##main", ImGuiTableColumnFlags_WidthStretch);

        for(size_t i = 0; i < Recents::kMaxRecents; i++) {
            // skip empty meepers
            const auto &entry = this->recents.recents[i];
            if(!entry) continue;

            // begin a new row
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            std::filesystem::path path(entry->path);

            std::time_t opened = std::chrono::system_clock::to_time_t(entry->lastOpened);
            const auto openedTm = localtime(&opened);

            char timeBuf[64];
            memset(&timeBuf, 0, sizeof(timeBuf));
            strftime(timeBuf, 63, "%c", openedTm);

            const auto str = f("{}\nLast Played: {}", path.filename().string(), timeBuf);

            if(ImGui::Selectable(str.c_str(), (this->selectedWorld == i))) {
                this->selectedWorld = i;
            }
        }

        ImGui::EndTable();
    }
}



/**
 * Opens a world at the given path. Errors are displayed, and the recents list is updated.
 */
void WorldSelector::openWorld(const std::string &path) {
    Logging::debug("Opening world file: {}", path);

    // ensure it exists

    // update the recents list
    this->recents.addPath(path);
    this->saveRecents();

    this->selectedWorld = -1;

    // TODO: open the world
}
