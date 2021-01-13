#include "WorldSelector.h"
#include "TitleScreen.h"
#include "gui/GameUI.h"

#include "io/PrefsManager.h"
#include "io/Format.h"

#include "world/FileWorldReader.h"
#include "world/generators/Terrain.h"
#include "world/WorldSource.h"

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
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <ctime>

using namespace gui::title;

const std::string WorldSelector::kPrefsKey = "ui.worldSelector.recents";

/**
 * Initializes a world selector.
 */
WorldSelector::WorldSelector(TitleScreen *_title) : title(_title) {
    // configure file dialogs
    igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".world", ImVec4(0,0.69,0,0.9));
}


/**
 * Loads the recents list from preferences.
 */
void WorldSelector::loadRecents() {
    // clear state
    memset(&this->newName, 0, kNameMaxLen);
    this->newSeed = 420;

    this->isCreateOpen = false;
    this->isErrorOpen = false;
    this->isFileDialogOpen = false;

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
        this->selectedWorld = -1;
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

    if(!this->isFileDialogOpen && !this->isErrorOpen && !this->isCreateOpen) {
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
        ImGuiFileDialog::Instance()->OpenModal("OpenWorld", "Choose World File", kWorldFilters, "");
        this->isFileDialogOpen = true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Create New...")) {
        ImGui::OpenPopup("New World");
        this->isCreateOpen = true;
    }


    if(this->selectedWorld >= 0) {
        // spacing
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));

        // open selection
        ImGui::SameLine();
        if(ImGui::Button("Open Selected")) {
            const auto &entry = this->recents.recents[this->selectedWorld];
            if(entry) {
                this->openWorld(entry->path);
            }
        }

        // remove from recents
        ImGui::SameLine();
        if(ImGui::Button("Remove Selected")) {
            this->recents.recents[this->selectedWorld] = std::nullopt;
            this->selectedWorld = -1;

            this->saveRecents();
        }
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Removes the selected item from the list of recently opened worlds. The world file will not be deleted.");
        }
    } 

    // file dialogs
    if(this->isFileDialogOpen) {
        ImGui::SetNextWindowSize(ImVec2(640, 420));
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

        if(ImGuiFileDialog::Instance()->FileDialog("OpenWorld", ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize)) {
            if(ImGuiFileDialog::Instance()->IsOk) {
                const auto path = ImGuiFileDialog::Instance()->GetFilePathName();
                this->openWorld(path);
            }

            ImGuiFileDialog::Instance()->CloseDialog("OpenWorld");
            this->isFileDialogOpen = false;
        }
    }

    if(this->isErrorOpen) {
        this->drawErrors(gui);
    }

    if(this->isCreateOpen) {
        this->drawCreate(gui);
    }

    // end
    ImGui::End();
}

/**
 * Displays any error messages.
 */
void WorldSelector::drawErrors(GameUI *gui) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::SetNextWindowSizeConstraints(ImVec2(450, 150), ImVec2(450, 320));

    ImGui::OpenPopup("Error");
    if(ImGui::BeginPopupModal("Error", &this->isErrorOpen, ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        // main
        ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
        ImGui::TextWrapped("Oops! We've run into a little bit of trouble.");
        ImGui::PopFont();

        // world name/path
        if(!this->errorFile.empty()) {
            ImGui::Bullet();
            ImGui::TextWrapped("World: %s", this->errorFile.c_str());
        }

        // detailed text
        if(!this->errorDetail.empty()) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
            if(ImGui::CollapsingHeader("Details")) {
                ImGui::TextWrapped("%s", this->errorDetail.c_str());
            }
        }

        // actions
        ImGui::Dummy(ImVec2(0,ImGui::GetTextLineHeight()));
        ImGui::Separator();
        ImGui::SetItemDefaultFocus();

        if(ImGui::Button("Dismiss")) {
            this->isErrorOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

/**
 * Sets an error message to be displayed.
 */
void WorldSelector::setError(const std::string &_path, const std::string &detail) {
    // std::filesystem::path path(_path);
    // this->errorFile = path.filename().string();
    this->errorFile = _path;
    this->errorDetail = detail;

    this->isErrorOpen = true;
    ImGui::OpenPopup("Error");
}

/**
 * Draws the recents table.
 */
void WorldSelector::drawRecentsList(GameUI *gui) {
    const auto tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_BordersOuter;
    const ImVec2 tableSize(-FLT_MIN, 520);

    if(ImGui::BeginTable("##recents", 1, tableFlags, tableSize)) {
        ImGui::TableSetupColumn("##main", ImGuiTableColumnFlags_WidthStretch);

        if(this->recents.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
            ImGui::TextUnformatted("No recent worlds available");
            ImGui::PopFont();
        } else {
            for(size_t i = 0; i < Recents::kMaxRecents; i++) {
                // skip empty meepers
                const auto &entry = this->recents.recents[i];
                if(!entry) continue;

                // begin a new row
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i);

                std::filesystem::path path(entry->path);

                std::time_t opened = std::chrono::system_clock::to_time_t(entry->lastOpened);
                const auto openedTm = localtime(&opened);

                char timeBuf[64];
                memset(&timeBuf, 0, sizeof(timeBuf));
                strftime(timeBuf, 63, "%c", openedTm);

                const auto str = f("{}\nLast Played: {}", path.filename().string(), timeBuf);

                if(ImGui::Selectable(str.c_str(), (this->selectedWorld == i), ImGuiSelectableFlags_AllowDoubleClick)) {
                    this->selectedWorld = i;

                    // open right away on double click
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        const auto &entry = this->recents.recents[i];
                        if(entry) this->openWorld(entry->path);
                    }
                }
                // context menu
                if(ImGui::BeginPopupContextItem("##context")) {
                    // remove from recents
                    if(ImGui::MenuItem("Remove World")) {
                        this->recents.recents[i] = std::nullopt;
                        this->saveRecents();
                    }
                    ImGui::EndPopup();
                }
                // tooltip
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Path: %s", entry->path.c_str());
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }
}

/**
 * Draws the modal window for the "create new world" function.
 */
void WorldSelector::drawCreate(GameUI *gui) {
    using namespace igfd;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::SetNextWindowSize(ImVec2(474, 274));

    if(!ImGui::BeginPopupModal("New World", nullptr, ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        return;
    }

    ImGui::PushItemWidth(320);

    // name
    ImGui::InputText("World Name", this->newName, kNameMaxLen);

    // generator options
    ImGui::InputInt("Generator Seed", &this->newSeed);
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The seed is an integer value that controls generation of new chunks.\nSeeds are signed 32-bit integers, meaning their range is -2147483648 to 2147483647. Values larger than this are truncated.");
    }

    // actions
    ImGui::Separator();
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(-FLT_MIN);
    ImGui::SetItemDefaultFocus();

    if(ImGui::Button("Cancel")) {
        this->isCreateOpen = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button("Create World")) {
        // validate args
        if(!strlen(this->newName)) {
            this->setError("", "You must enter a world name.");
        }
        // all validations passed; open the file dialog
        else {
            const auto name = f("{}.world", this->newName);
            ImGuiFileDialog::Instance()->OpenModal("SaveWorld", "Save World File", kWorldFilters,
                    ".", this->newName, 1, nullptr, ImGuiFileDialogFlags_ConfirmOverwrite);
            this->isFileDialogOpen = true;
        }
    }

    ImGui::PopItemWidth();

    // handle the save dialog
    ImGui::SetNextWindowSize(ImVec2(640, 420));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    if(ImGuiFileDialog::Instance()->FileDialog("SaveWorld", ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize)) {
        if(ImGuiFileDialog::Instance()->IsOk) {
            const auto path = ImGuiFileDialog::Instance()->GetFilePathName();
            try {
                this->createWorld(path);
            } catch(std::exception &e) {
                Logging::error("Failed to create world: {}", e.what());
                this->setError(path, f("Failed to create the world file: {}", e.what()));
                goto beach;
            }

            // close the modal; if we get here, saving was successful
            this->isCreateOpen = false;
            ImGui::CloseCurrentPopup();
        }

beach:;
        // close file dialog
        ImGuiFileDialog::Instance()->CloseDialog("SaveWorld");
        this->isFileDialogOpen = false;
    }

    ImGui::EndPopup();
}



/**
 * Creates a new world and opens it.
 */
void WorldSelector::createWorld(const std::string &path, const bool open) {
    Logging::trace("Creating new world: {}", path);

    // create world
    auto file = std::make_shared<world::FileWorldReader>(path, true);
    auto gen = std::make_shared<world::Terrain>(this->newSeed);
    auto source = std::make_shared<world::WorldSource>(file, gen);

    // save seed/generator settings in world file
    auto p = file->setWorldInfo("generator.seed", f("{:d}", this->newSeed));
    p.get_future().get();

    // open the world if desired
    if(open) {
        // if created successfully, add to recents list
        this->recents.addPath(path);
        this->saveRecents();

        // then, open it
        this->title->openWorld(source);
    }
}

/**
 * Opens a world at the given path. Errors are displayed, and the recents list is updated.
 */
void WorldSelector::openWorld(const std::string &path) {
    Logging::debug("Opening world file: {}", path);


    // ensure it exists
    std::filesystem::path p(path);

    if(!std::filesystem::exists(p)) {
        Logging::error("Failed to open world {}: file doesn't exist", path);
        this->setError(path, "World file does not exist. Ensure it's at the expected location, you have permission to access it, and try again.");
        return;
    }

    // create a world source
    std::shared_ptr<world::WorldSource> source = nullptr;

    try {
        // load the file
        auto file = std::make_shared<world::FileWorldReader>(path, false);

        // set up the appropriate generator
        auto seedProm = file->getWorldInfo("generator.seed");
        auto seedData = seedProm.get_future().get();

        int32_t seed = 420;
        if(!seedData.empty()) {
            // seed is stored as a string
            const std::string seedStr(seedData.begin(), seedData.end());
            seed = stoi(seedStr);
        } else {
            Logging::warn("Failed to load seed for world {}; using default value ${:X}", path, seed);
        }

        auto gen = std::make_shared<world::Terrain>(seed);

        // lastly, combine the file and generator to a world source
        source = std::make_shared<world::WorldSource>(file, gen);
    } catch(std::exception &e) {
        Logging::error("Failed to open world {}: {}", path, e.what());
        this->setError(path, f("An error occurred while reading the world file: {}", e.what()));
        return;
    }

    // update the recents list
    this->recents.addPath(path);
    this->saveRecents();

    this->selectedWorld = -1;

    // if we get here, we should actually go and open the world :D
    this->title->openWorld(source);
}
