#include "WorldDebugger.h"
#include "gui/GameUI.h"
#include "gui/Loaders.h"
#include "world/WorldReader.h"
#include "world/FileWorldReader.h"
#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"

#include <Logging.h>
#include "io/Format.h"

#include <mutils/time/profiler.h>
#include <imgui.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

using namespace world;

/**
 * Starts the worker thread on initialization.
 */
WorldDebugger::WorldDebugger() {
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(&WorldDebugger::workerMain, this);
}

/**
 * Makes sure the work thread gets shut down.
 */
WorldDebugger::~WorldDebugger() {
    this->workerRun = false;
    this->sendWorkerNop();

    this->worker->join();
}

/**
 * Main rendering function for the world debugger
 */
void WorldDebugger::draw(gui::GameUI *ui) {
    // short circuit drawing if not visible
    if(!ImGui::Begin("World Debugger", &this->isDebuggerOpen)) {
        ImGui::End();
        return;
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

    // chunk actions
    ImGui::PushFont(ui->getFont(gui::GameUI::kBoldFontName));
    ImGui::TextUnformatted("Chunks");
    ImGui::PopFont();
    ImGui::Separator();

    if(this->world) {
        this->drawChunkUi(ui);
    } else {
        ImGui::PushFont(ui->getFont(gui::GameUI::kItalicFontName));
        ImGui::TextUnformatted("Load a world to access the chunk editor");
        ImGui::PopFont();
    }

    // file type
    auto file = dynamic_pointer_cast<FileWorldReader>(this->world);
    if(file) {
        ImGui::PushFont(ui->getFont(gui::GameUI::kBoldFontName));
        ImGui::TextUnformatted("File World Reader");
        ImGui::PopFont();
        ImGui::Separator();

        this->drawFileWorldUi(ui, file);
    }

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

    // busy indicator
    if(!this->worldError && this->isBusy) {
        ImGui::OpenPopup("Working");

        // center the modal
        ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::BeginPopupModal("Working", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushFont(ui->getFont(gui::GameUI::kBoldFontName));
            ImGui::Text("Please wait... this should only take a moment");
            ImGui::PopFont();

            const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            ImGui::Spinner("##spin", 9, 3, col);
            ImGui::SameLine();

            ImGui::TextWrapped("Current step: %s", this->busyText.c_str());

            ImGui::EndPopup();
        }
    }

    // world loading errors
    if(this->worldError) {
        ImGui::OpenPopup("Loading Error");

        // center the modal
        ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::BeginPopupModal("Loading Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushFont(ui->getFont(gui::GameUI::kBoldFontName));
            ImGui::Text("Ooops! Something got a bit fucked.");
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



/**
 * Draws the UI for the world file reader.
 *
 * This allows specifically to view the type id -> uuid map, and run raw queries.
 */
void WorldDebugger::drawFileWorldUi(gui::GameUI *ui, std::shared_ptr<FileWorldReader> file) {
    // begin tab ui
    if(!ImGui::BeginTabBar("file")) {
        return;
    }

    // type map
    if(ImGui::BeginTabItem("Type Map")) {
        this->drawFileTypeMap(ui, file);
        ImGui::EndTabItem();
    }

    // finish tab bar
    ImGui::EndTabBar();
}
/**
 * Draws the table to display the mapping between block id and block type uuid
 */
void WorldDebugger::drawFileTypeMap(gui::GameUI *ui, std::shared_ptr<FileWorldReader> file) {
    if(!ImGui::BeginTable("typeMap", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch)) {
        return;
    }

    // headers
    ImGui::TableSetupColumn("Local ID", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("UUID");
    ImGui::TableHeadersRow();

    // draw each row
    for(const auto &[key, value] : file->blockIdMap) {
        ImGui::TableNextRow();
        ImGui::PushID(key);
        
        ImGui::TableNextColumn();
        ImGui::Text("%04x", key);
        
        ImGui::TableNextColumn();
        ImGui::Text("%s", uuids::to_string(value).c_str());

        ImGui::PopID();
    }

    ImGui::EndTable();
}


/**
 * Draws the UI controls to do with modifying with chunks.
 *
 * Primarily, this allows generating chunks and writing them to arbitrary chunks; as well as
 * reading chunk data and displaying an extents map.
 */
void WorldDebugger::drawChunkUi(gui::GameUI *ui) {
    // string constants
    const static size_t kNumFillTypes = 1;
    const static char *kFillTypes[kNumFillTypes] = {
        "Solid (y <= 32)",
    };

    // begin tab ui
    if(!ImGui::BeginTabBar("chunks")) {
        return;
    }

    // type map
    if(ImGui::BeginTabItem("Write")) {
        ImGui::PushItemWidth(150);

        // X/Z coord for the chunk to write
        ImGui::DragInt2("Location", this->chunkState.writeCoord);

        // fill type
        if(ImGui::BeginCombo("Fill Type", kFillTypes[this->chunkState.fillType])) {
            for(size_t j = 0; j < kNumFillTypes; j++) {
                const bool isSelected = (this->chunkState.fillType == j);

                if (ImGui::Selectable(kFillTypes[j], isSelected)) {
                    this->chunkState.fillType = j;
                }
                if(isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // write button
        ImGui::BulletText("%s", "Note: Existing chunk data will be overwritten!");
        if(ImGui::Button("Write Chunk")) {
            this->isBusy = true;
            this->busyText = "Writing chunk";

            // perform this work in the background
            this->workQueue.enqueue([&]{
                // build the chunk and fill it
                auto chunk = std::make_shared<Chunk>();
                chunk->worldPos = glm::vec2(this->chunkState.writeCoord[0],
                        this->chunkState.writeCoord[1]);

                {
                    PROFILE_SCOPE(FillChunk);
                    this->fillChunkSolid(chunk, 32);
                }

                // then request to write it out
                try {
                    PROFILE_SCOPE(PutChunk);
                    auto promise = this->world->putChunk(chunk);
                    promise.get_future().get();
                } catch(std::exception &e) {
                    this->worldError = std::make_unique<std::string>(f("putChunk() failed:\n{}", e.what()));
                }

                this->isBusy = false;
            });
        }

        // finish write section
        ImGui::PopItemWidth();
        ImGui::EndTabItem();
    }

    // finish tab bar
    ImGui::EndTabBar();
}



/**
 * Worker thread main loop
 */
void WorldDebugger::workerMain() {
    int err;

    MUtils::Profiler::NameThread("World Debugger");

    // wait for work to come in
    std::function<void(void)> item;
    while(this->workerRun) {
        this->workQueue.wait_dequeue(item);

        PROFILE_SCOPE(Callout);
        item();
    }
}

/**
 * Sends a no-op to the worker thread to wake it up.
 */
void WorldDebugger::sendWorkerNop() {
    this->workQueue.enqueue([&]{ /* nothing */ });
}

/**
 * Fills a solid pile of blocks into the chunk up to the given Y level.
 */
void WorldDebugger::fillChunkSolid(std::shared_ptr<Chunk> chunk, size_t yMax) {
    chunk->meta["generator.name"] = "WorldDebugger";
}

