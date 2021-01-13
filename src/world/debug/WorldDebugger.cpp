#include "WorldDebugger.h"
#include "gui/GameUI.h"
#include "gui/Loaders.h"
#include "world/WorldReader.h"
#include "world/FileWorldReader.h"
#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "render/scene/SceneRenderer.h"
#include "render/chunk/WorldChunk.h"

#include <Logging.h>
#include "io/Format.h"

#include <uuid.h>
#include <mutils/time/profiler.h>
#include <imgui.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

extern std::shared_ptr<render::SceneRenderer> gSceneRenderer;

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
    ImGui::Text("%p", (void *) this->world.get());
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
        if(ImGui::CollapsingHeader("File Reader")) {
            this->drawFileWorldUi(ui, file);
        }
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

    // handle drawing chunk viewer
    if(this->isChunkViewerOpen) {
        this->drawChunkViewer(ui);
    }
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
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 8);
    if(!ImGui::BeginTable("typeMap", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableColumnFlags_WidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
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
        ImGui::Text("0x%04x", key);
        
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
    // begin tab ui
    if(!ImGui::BeginTabBar("chunks")) {
        return;
    }

    if(ImGui::BeginTabItem("Read")) {
        this->drawChunkReadUi(ui);
        ImGui::EndTabItem();
    }
    if(ImGui::BeginTabItem("Write")) {
        this->drawChunkWriteUi(ui);
        ImGui::EndTabItem();
    }

    // finish tab bar
    ImGui::EndTabBar();
}

/**
 * Renders the chunk reading tab contents.
 */
void WorldDebugger::drawChunkReadUi(gui::GameUI *ui) {
    // default item width
    ImGui::PushItemWidth(150);

    // X/Z coord for the chunk to read
    ImGui::DragInt2("Location", this->chunkState.readCoord);

    // button
    if(ImGui::Button("Read Chunk")) {
        this->isBusy = true;
        this->busyText = "Reading chunk";

        // perform this work in the background
        this->workQueue.enqueue([&]{
            try {
                PROFILE_SCOPE(GetChunk);
                auto promise = this->world->getChunk(this->chunkState.readCoord[0], this->chunkState.readCoord[1]);

                this->chunk = promise.get_future().get();

                this->resetChunkViewer();
                this->isChunkViewerOpen = true;
            } catch(std::exception &e) {
                this->worldError = std::make_unique<std::string>(f("getChunk() failed:\n{}", e.what()));
            }

            this->isBusy = false;
        });    
    }

    // finish
    ImGui::PopItemWidth();
}


/**
 * Renders the chunk write tab contents
 */
void WorldDebugger::drawChunkWriteUi(gui::GameUI *ui) {
    // string constants
    const static size_t kNumFillTypes = 2;
    const static char *kFillTypes[kNumFillTypes] = {
        "Solid (y <= 32)",
        "Sphere (d = 32)"
    };

    // default item width
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

    // fill level
    ImGui::DragInt("Fill Y level", &this->chunkState.fillLevel, 1, 0, 255);

    // whether we write block properties
    ImGui::Checkbox("Write Block Properties", &this->chunkState.writeBlockProps);

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

                if(this->chunkState.fillType == 0) {
                    this->fillChunkSolid(chunk, this->chunkState.fillLevel);
                } else if(this->chunkState.fillType == 1) {
                    this->fillChunkSphere(chunk, this->chunkState.fillLevel);
                }
            }

            this->chunk = chunk;
            this->resetChunkViewer();
            this->isChunkViewerOpen = true;

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
}



/**
 * Draws the chunk viewer.
 */
void WorldDebugger::drawChunkViewer(gui::GameUI *ui) {
    // short circuit drawing if not visible
    if(!ImGui::Begin("Chunk Viewer", &this->isChunkViewerOpen)) {
        ImGui::End();
        return;
    }

    // error message if no chunk loaded
    if(!this->chunk) {
        ImGui::PushFont(ui->getFont(gui::GameUI::kItalicFontName));
        ImGui::TextUnformatted("Select a chunk to view in the world debugger.");
        ImGui::PopFont();

        ImGui::End();
        return;
    }

    // actions
    if(ImGui::Button("Render")) {
        /*auto world = dynamic_pointer_cast<render::WorldChunk>(gSceneRenderer->chunks[0]);
        if(!world) {
            this->worldError = std::make_unique<std::string>("Failed to get world chunk (for drawing)");
        } else {
            world->setChunk(this->chunk);
        }*/
    }
    ImGui::Separator();

    // main details of chunk
    ImGui::TextUnformatted("Instance: ");
    ImGui::SameLine();
    ImGui::Text("%p", (void *) this->chunk.get());

    ImGui::TextUnformatted("Metadata: ");
    ImGui::SameLine();
    ImGui::Text("%zu chunk / %zu block", this->chunk->meta.size(), this->chunk->blockMeta.size());

    ImGui::TextUnformatted("Slices: ");
    ImGui::SameLine();
    ImGui::Text("%zu", this->chunk->slices.size());

    // chunk metadata
    if(ImGui::CollapsingHeader("Chunk Metadata")) {
        if(!this->chunk->meta.empty()) {
            this->drawChunkMeta(ui);
        } else {
            ImGui::PushFont(ui->getFont(gui::GameUI::kItalicFontName));
            ImGui::TextUnformatted("No data available");
            ImGui::PopFont();
        }
    }

    // slice ID maps
    if(ImGui::CollapsingHeader("Slice ID Maps")) {
        if(!this->chunk->sliceIdMaps.empty()) {
            ImGui::PushItemWidth(74);

            if(ImGui::BeginCombo("Map Index", f("{:d}", this->viewerState.currentIdMap).c_str())) {
                for(size_t j = 0; j < this->chunk->sliceIdMaps.size(); j++) {
                    const bool isSelected = (this->viewerState.currentIdMap == j);

                    if (ImGui::Selectable(f("{:d}", j).c_str(), isSelected)) {
                        this->viewerState.currentIdMap = j;
                    }
                    if(isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            this->drawChunkIdMap(ui);
        } else {
            ImGui::PushFont(ui->getFont(gui::GameUI::kItalicFontName));
            ImGui::TextUnformatted("No data available");
            ImGui::PopFont();
        }
    }

    // rows
    if(ImGui::CollapsingHeader("Slice Data")) {
        this->drawChunkRows(ui);
    }

    // metadata
    if(ImGui::CollapsingHeader("Block Metadata")) {
        this->drawBlockInfo(ui);
    }

    // finish window
    ImGui::End();
}

/**
 * Draws the chunk metadata table.
 */
void WorldDebugger::drawChunkMeta(gui::GameUI *ui) {
    // max 5 rows before we scroll
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 5);
    if(!ImGui::BeginTable("meta", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableColumnFlags_WidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    // headers
    ImGui::TableSetupColumn("Key");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();

    // draw each row
    for(const auto &[key, value] : this->chunk->meta) {
        ImGui::TableNextRow();
        ImGui::PushID(key.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(key.c_str());

        ImGui::TableNextColumn();
        this->printMetaValue(value);

        ImGui::PopID();
    }

    ImGui::EndTable();
}

/**
 * Draws the currently selected chunk block id map.
 */
void WorldDebugger::drawChunkIdMap(gui::GameUI *ui) {
    // max 5 rows before we scroll
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 5);
    if(!ImGui::BeginTable("idMap", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableColumnFlags_WidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    // headers
    ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 18);
    ImGui::TableSetupColumn("UUID");
    ImGui::TableHeadersRow();

    // draw each row
    const auto &map = this->chunk->sliceIdMaps[this->viewerState.currentIdMap];

    for(size_t i = 0; i < map.idMap.size(); i++) {
        // skip nil UUIDs
        const auto &uuid = map.idMap[i];
        if(uuid.is_nil()) continue;

        // otherwise, print it
        ImGui::TableNextRow();
        ImGui::PushID(i);

        ImGui::TableNextColumn();
        ImGui::Text("%02zx", i);

        ImGui::TableNextColumn();
        ImGui::Text("%s", uuids::to_string(uuid).c_str());

        ImGui::PopID();
    }

    ImGui::EndTable();
}

/**
 * UI for viewing chunk rows
 */
void WorldDebugger::drawChunkRows(gui::GameUI *ui) {
    // draw the slice selector
    ImGui::PushItemWidth(74);
    ImGui::DragInt("Slice (Y)", &this->viewerState.currentSlice, 1, 0, 255);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::TextUnformatted("Instance: ");
    ImGui::SameLine();

    auto slice = this->chunk->slices[this->viewerState.currentSlice];
    ImGui::Text("%p", (void *) slice);

    // bail if there is no slice
    if(!slice) {
        ImGui::PushFont(ui->getFont(gui::GameUI::kItalicFontName));
        ImGui::TextUnformatted("No data available");
        ImGui::PopFont();
        return;
    }

    // draw the row selector
    ImGui::PushItemWidth(74);
    ImGui::DragInt("Row (Z)", &this->viewerState.currentRow, 1, 0, 255);

    ImGui::SameLine();
    ImGui::TextUnformatted("Instance: ");
    ImGui::SameLine();

    auto row = slice->rows[this->viewerState.currentRow];
    ImGui::Text("%p", (void *) row);

    if(row) {
        ImGui::TextUnformatted("ID Map: ");
        ImGui::SameLine();
        ImGui::Text("0x%02x", row->typeMap);

        // draw the type of the row and detail about it
        auto sparse = dynamic_cast<ChunkSliceRowSparse *>(row);
        auto dense = dynamic_cast<ChunkSliceRowDense *>(row);

        ImGui::TextUnformatted("Type: ");
        ImGui::SameLine();

        // sparse
        if(sparse) {
            ImGui::TextUnformatted("Sparse");
            this->drawRowInfo(ui, sparse);
        }
        // dense
        else if(dense) {
            ImGui::TextUnformatted("Dense");
            this->drawRowInfo(ui, dense);
        }
        // none
        else {
            ImGui::TextUnformatted("??? Unknown (this should not happen)");
        }
        ImGui::PopItemWidth();
    }
}

/**
 * Draws info for a sparse chunk row.
 */
void WorldDebugger::drawRowInfo(gui::GameUI *ui, ChunkSliceRowSparse *sparse) {
    // base ID
    ImGui::TextUnformatted("Default Block ID: ");
    ImGui::SameLine();
    ImGui::Text("0x%02x", sparse->defaultBlockId);

    // sparse value overrides
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 5);
    if(ImGui::BeginTable("sparseVals", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableColumnFlags_WidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {

        // headers
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 18);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        // draw each row
        for(const auto value: sparse->storage) {
            ImGui::TableNextRow();
            ImGui::PushID(value);

            ImGui::TableNextColumn();
            ImGui::Text("%d", (value & 0xFF00) >> 8);

            ImGui::TableNextColumn();
            ImGui::Text("0x%02x", value & 0x00FF);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

/**
 * UI for showing a dense row's storage.
 */
void WorldDebugger::drawRowInfo(gui::GameUI *ui, ChunkSliceRowDense *dense) {
    // TODO: view array
}

/**
 * Prints a metadata value.
 */
void WorldDebugger::printMetaValue(const std::variant<std::monostate, bool, std::string, double, int64_t> &val) {
    if(std::holds_alternative<std::string>(val)) {
        ImGui::TextUnformatted(std::get<std::string>(val).c_str());
    } else if(std::holds_alternative<bool>(val)) {
        ImGui::TextUnformatted(std::get<bool>(val) ? "true" : "false");
    } else if(std::holds_alternative<double>(val)) {
        ImGui::Text("%g", std::get<double>(val));
    } else if(std::holds_alternative<int64_t>(val)) {
        ImGui::Text("%lld", std::get<int64_t>(val));
    }
}

/**
 * Draws the block metadata.
 */
void WorldDebugger::drawBlockInfo(gui::GameUI *ui) {
    // bail if shit is empty
    if(this->chunk->blockMeta.empty()) {
        ImGui::PushFont(ui->getFont(gui::GameUI::kItalicFontName));
        ImGui::TextUnformatted("No data available");
        ImGui::PopFont();

        return;
    }

    ImGui::PushItemWidth(74);

    // start the table boi
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 15);
    if(!ImGui::BeginTable("blockMeta", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableColumnFlags_WidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    // headers
    ImGui::TableSetupColumn("Position(YZX)/Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
    ImGui::TableHeadersRow();

    // for each block metadata entry...
    for(const auto &[pos, blockMeta] : this->chunk->blockMeta) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // start a tree node to contain its children
        const auto &data = blockMeta.meta;
        if(!data.empty()) {
            const auto posStr = f("({:d}, {:d}, {:d})", ((pos & 0xFF0000) >> 16), 
                    (pos & 0xFF00) >> 8, (pos & 0xFF));
            bool open = ImGui::TreeNodeEx(posStr.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

            ImGui::TableNextColumn();
            ImGui::TextDisabled("%lu key(s)", data.size());

            // if open, draw all the key/value pairs
            if(open) {
                for(const auto &[key, value] : data) {
                    const auto &keyStr = chunk->blockMetaIdMap[key];

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(-1);
                    ImGui::TreeNodeEx(keyStr.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();
                    this->printMetaValue(value);
                }
                ImGui::TreePop();
            }
        }
    }

    // end table boi
    ImGui::EndTable();

    ImGui::PopItemWidth();
}


/**
 * Reset the chunk viewer state.
 */
void WorldDebugger::resetChunkViewer() {
    this->viewerState = ChunkViewerState();
}


/**
 * Worker thread main loop
 */
void WorldDebugger::workerMain() {
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
 * Prepares a chunk by setting its id maps.
 */
void WorldDebugger::prepareChunkMaps(std::shared_ptr<Chunk> chunk) {
    /// UUIDs of blocks
    static const std::array<uuids::uuid::value_type, 16> kBlockIdsRaw[4] = {
        {0x71, 0x4a, 0x92, 0xe3, 0x29, 0x84, 0x4f, 0x0e, 0x86, 0x9e, 0x14, 0x16, 0x2d, 0x46, 0x27, 0x60},
        {0x2b, 0xe6, 0x86, 0x12, 0x13, 0x3b, 0x40, 0xc6, 0x84, 0x36, 0x18, 0x9d, 0x4b, 0xd8, 0x7a, 0x4e},
        {0xf2, 0xca, 0x67, 0x5d, 0x92, 0x5f, 0x4b, 0x1e, 0x8d, 0x6a, 0xa6, 0x66, 0x45, 0x89, 0xff, 0xe5},
        {0xfe, 0x35, 0x39, 0xd4, 0xd6, 0x96, 0x4b, 0x04, 0x8e, 0x34, 0xa6, 0x5f, 0xd0, 0xb4, 0x4e, 0x7d}
    };
    uuids::uuid kBlockIds[4];
    for(size_t i = 0; i < 4; i++) {
        kBlockIds[i] = uuids::uuid(kBlockIdsRaw[i].begin(), kBlockIdsRaw[i].end());
    }

    chunk->meta["generator.name"] = "WorldDebugger";

    // build the slice ID -> UUID map
    ChunkRowBlockTypeMap idMap;
    for(size_t i = 0; i < 4; i++) {
        idMap.idMap[i] = kBlockIds[i];
    }
    chunk->sliceIdMaps.push_back(idMap);
}

/**
 * Fills a solid pile of blocks into the chunk up to the given Y level.
 */
void WorldDebugger::fillChunkSolid(std::shared_ptr<Chunk> chunk, size_t yMax) {
    Logging::debug("Filling chunk {} with solid data to y {}", (void *) chunk.get(), yMax);
    this->prepareChunkMaps(chunk);

    // metadata keys
    chunk->blockMetaIdMap[1] = "me.tseifert.cubeland.test";
    chunk->blockMetaIdMap[2] = "me.tseifert.cubeland.strain";
    chunk->blockMetaIdMap[3] = "me.tseifert.cubeland.isFucked";

    // For each Y layer, create a slice
    for(size_t y = 0; y < yMax; y++) {
        auto slice = new ChunkSlice;

        // create a sparse row for each column
        for(size_t z = 0; z < 256; z++) {
            auto row = chunk->allocRowSparse();
            row->defaultBlockId = 1;

            // this makes a diagonal stripe
            for(size_t x = 0; x < 256; x++) {
                if(x == y) {
                    row->storage[x] = 0;

                    // literally fuck me in the god damn ass
                    BlockMeta fucker;
                    fucker.meta[1] = 420.69;

                    if(y & 0x01 && (z % 32) == 0 && this->chunkState.writeBlockProps) {
                        chunk->blockMeta[((y & 0xFF) << 16) | ((z & 0xFF) << 8) | (x & 0xFF)] = fucker;
                    }
                } else if(x == (z / 2)) {
                    row->storage[x] = 2;

                    BlockMeta fucker;
                    fucker.meta[2] = "Sativa";
                    if(y & 0x01 && (z % 32) == 0 && this->chunkState.writeBlockProps) {
                        chunk->blockMeta[((y & 0xFF) << 16) | ((z & 0xFF) << 8) | (x & 0xFF)] = fucker;
                    }
                } else if ((x + (z & 0xF)) % 16 == 2) {
                    BlockMeta fucker;
                    fucker.meta[3] = false;

                    if(y % 4 == 3) {
                        fucker.meta[2] = "indica";
                    }

                    if(y & 0x01 && (z % 32) == 0 && this->chunkState.writeBlockProps) {
                        chunk->blockMeta[((y & 0xFF) << 16) | ((z & 0xFF) << 8) | (x & 0xFF)] = fucker;
                    }
                }
            }

            // assign row to the slice
            slice->rows[z] = row;
        }

        // attach it to the chunk
        chunk->slices[y] = slice;
    }
}

/**
 * Fills a chunk with a roughly spherical meeper.
 */
void WorldDebugger::fillChunkSphere(std::shared_ptr<Chunk> chunk, size_t radius) {
    this->prepareChunkMaps(chunk);

    for(size_t y = 0; y < (radius * 2); y++) {
        auto slice = new ChunkSlice;
        bool sliceWritten = false;

        // create a sparse row for each column
        for(size_t z = 0; z < 256; z++) {
            auto row = chunk->allocRowSparse();
            row->defaultBlockId = 0;
            bool written = false;

            for(size_t x = 0; x < 256; x++) {
                const float WORLD_SIZE = (float) radius;
                if (sqrt((float) (x-WORLD_SIZE/2)*(x-WORLD_SIZE/2) + (y-WORLD_SIZE/2)*(y-WORLD_SIZE/2) + (z-WORLD_SIZE/2)*(z-WORLD_SIZE/2)) < WORLD_SIZE/2) {
                    row->storage[x] = 1;
                    written = true;
                }
            }

            // assign row to the slice
            if(written) {
                slice->rows[z] = row;
                sliceWritten = true;
            } else {
                delete row;
            }
        }

        // attach it to the chunk
        if(sliceWritten) {
            chunk->slices[y] = slice;
        } else {
            delete slice;
        }
    }
}

