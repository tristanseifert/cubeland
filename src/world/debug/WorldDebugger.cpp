#include "WorldDebugger.h"
#include "gui/GameUI.h"
#include "../WorldReader.h"
#include "../FileWorldReader.h"

#include <Logging.h>
#include "io/Format.h"

#include <mutils/time/profiler.h>
#include <imgui.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

using namespace world;

/**
 * Main rendering function for the world debugger
 */
void WorldDebugger::draw(gui::GameUI *ui) {
    // short circuit drawing if not visible
    if(!ImGui::Begin("World Debugger", &this->isDebuggerOpen)) {
        goto done;
    }

    // toolbar section
    if(this->world == nullptr) {
        if(ImGui::Button("Open")) {
            igfd::ImGuiFileDialog::Instance()->OpenDialog("WorldDbgOpen", "Open World", ".world", ".");
        }
    } else {
        if(ImGui::Button("Close")) {
            this->world = nullptr;
        }
        ImGui::SameLine();
        if(ImGui::Button("Query test")) {
            this->loadWorldInfo();
        }
    }

    ImGui::Separator();

    // current loader
    ImGui::TextUnformatted("World: ");
    ImGui::SameLine();
    ImGui::Text("%p", this->world.get());
    ImGui::TextUnformatted("Implementation: ");
    ImGui::SameLine();
    ImGui::Text("%s", typeid(this->world.get()).name());

    // actions

    // handle open panel
    igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".world", ImVec4(1,1,0, 0.9));
    if(igfd::ImGuiFileDialog::Instance()->FileDialog("WorldDbgOpen")) {
        // OK button clicked -> file selected 
        if(igfd::ImGuiFileDialog::Instance()->IsOk == true) {
            const auto filePath = igfd::ImGuiFileDialog::Instance()->GetFilePathName();
            Logging::info("Opening world from: {}", filePath);

            try {
                this->world = std::make_shared<FileWorldReader>(filePath);
                this->loadWorldInfo();
            } catch(std::exception &e) {
                Logging::error("Failed to open world: {}", e.what());
                this->worldError = std::make_unique<std::string>(f("FileWorldReader::FileWorldReader() failed:\n{}", e.what()));
            }
        }

        // close the dialog
        igfd::ImGuiFileDialog::Instance()->CloseDialog("WorldDbgOpen");
    }

    // world loading errors
    if(this->worldError) {
        ImGui::OpenPopup("Loading Error");

        // center the modal
        ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::BeginPopupModal("Loading Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushFont(ui->getFont(gui::GameUI::kBoldFontName));
            ImGui::Text("Something went wrong while loading the world file.");
            ImGui::PopFont();

            ImGui::TextWrapped("%s", this->worldError->c_str());

            ImGui::Separator();

            ImGui::SetItemDefaultFocus();
            if(ImGui::Button("Dismiss")) {
                this->worldError = nullptr;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

done:;
    ImGui::End();
}

/**
 * Loads world info to update the UI with.
 */
void WorldDebugger::loadWorldInfo() {
    try {
        auto reader = std::dynamic_pointer_cast<FileWorldReader>(this->world);

        auto prom = reader->getDbSize();
        auto size = prom.get_future().get();
        Logging::trace("Db size: {}", size);

        auto have00 = this->world->chunkExists(0, 0).get_future().get();
        auto have01 = this->world->chunkExists(0, 1).get_future().get();

        Logging::trace("Chunk (0,0): {}, (0,1): {}", have00, have01);

        auto extents = this->world->getWorldExtents().get_future().get();
        Logging::trace("World extents (Xmin, Xmax, Zmin, Zmax): {}", extents);
    } catch(std::exception &e) {
        Logging::error("Failed to get db size: {}", e.what());
    }

}
