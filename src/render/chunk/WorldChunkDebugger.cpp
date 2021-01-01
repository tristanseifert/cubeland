#include "WorldChunkDebugger.h"
#include "WorldChunk.h"
#include "Globule.h"

#include "world/chunk/Chunk.h"

#include <Logging.h>
#include "io/Format.h"

#include <glm/gtx/string_cast.hpp>
#include <imgui.h>

using namespace render::chunk;

/**
 * Initializes the world chunk debugger.
 */
WorldChunkDebugger::WorldChunkDebugger(WorldChunk *_chunk) : chunk(_chunk) {

}

/**
 * Draws the chunk debugger UI.
 */
void WorldChunkDebugger::draw() {
    // title
    std::string title = "WorldChunk";

    if(this->chunk && this->chunk->chunk) {
        title = f("WorldChunk {}", this->chunk->chunk->worldPos);
    }

    // short circuit drawing if not visible
    if(!ImGui::Begin(title.c_str(), &this->isDebuggerOpen)) {
        ImGui::End();
        return;
    }

    // yeet
    ImGui::Checkbox("Draw Wireframe", &this->chunk->drawWireframe);

    // exposure map
    if(ImGui::CollapsingHeader("Exposure Map")) {
        this->drawExposureMap();
    }
    if(ImGui::CollapsingHeader("Highlights")) {
        this->drawHighlightsList();
    }
    if(ImGui::CollapsingHeader("Globules")) {
        this->drawGlobules();
    }

    // done
    ImGui::End();

    // perform non-UI updating
    if(this->exposureMapState.updateHighlights) {
        this->updateExposureMapHighlights();
    }
}

/**
 * Render the exposure map controls.
 */
void WorldChunkDebugger::drawExposureMap() {
    // auto &s = this->exposureMapState;

    // selecting the map to use
    ImGui::PushItemWidth(74);
    if(ImGui::DragInt("Slice (Y)", &this->exposureMapState.mapY, 1, 0, 255)) {
        this->exposureMapState.updateHighlights = true;
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(74);
    if(ImGui::DragInt("Row (Z)", &this->exposureMapState.mapZ, 1, 0, 255)) {
        this->exposureMapState.updateHighlights = true;
    }
    ImGui::PopItemWidth();

    if(ImGui::Checkbox("Draw highlight", &this->exposureMapState.highlight)) {
        this->exposureMapState.updateHighlights = true;
    }

    /*
    // draw the rectangulars
    auto list = ImGui::GetWindowDrawList();
    auto cursor = ImGui::GetCursorScreenPos();

    list->AddRectFilled(cursor, ImVec2(cursor.x + (256 * 2) + 1, cursor.y+2+4), IM_COL32_WHITE);

    size_t offset = ((s.mapY & 0xFF) << 16) | ((s.mapZ & 0xFF) << 8);
    for(size_t x = 0; x < 256; x++) {
        ImVec2 p = cursor;
        p.x += 1+ x*2;
        p.y += 1;
        ImVec2 p2 = p;
        p2.x += 2;
        p2.y += 4;

        bool isSet = this->chunk->exposureMap[offset+x];

        if(isSet) {
            list->AddRectFilled(p, p2, IM_COL32(0,255,0,255));
        } else {
            list->AddRectFilled(p, p2, IM_COL32(255,0,0,255));
        }
    }

    ImGui::Dummy(ImVec2(0, 10));*/
}

/**
 * Updates the exposure map highlight region.
 */
void WorldChunkDebugger::updateExposureMapHighlights() {
    auto &s = this->exposureMapState;

    // remove existing highlight
    if(!s.highlight) {
        if(s.highlightId) {
            this->chunk->removeHighlight(s.highlightId);
            s.highlightId = 0;
        }
    }
    // create or update an existing one
    else {
        // remove existing
        if(s.highlightId) {
            this->chunk->removeHighlight(s.highlightId);
            s.highlightId = 0;
        }

        // create highlight for the entire row of shit
        glm::vec3 start(0, s.mapY, s.mapZ), end(255, s.mapY + 1, s.mapZ + 1);
        s.highlightId = this->chunk->addHighlight(start, end);
    }

    s.updateHighlights = false;
}



/**
 * Draws the list of all highlights.
 */
void WorldChunkDebugger::drawHighlightsList() {
    ImGui::TextUnformatted("Highlights: ");
    ImGui::SameLine();
    ImGui::Text("%lu", this->chunk->highlightData.size());

    // list the instance buffer contents
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 5);
    if(!ImGui::BeginTable("highlights", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 48);
    ImGui::TableSetupColumn("Transform Matrix");
    ImGui::TableHeadersRow();

    size_t i = 0;
    for(const auto &info : this->chunk->highlightData) {
        ImGui::TableNextRow();
        ImGui::PushID(i);

        ImGui::TableNextColumn();
        ImGui::Text("0x%zx", i);

        // print the transform matrix. this kinda suck
        ImGui::TableNextColumn();
        const auto str = glm::to_string(info.transform);
        ImGui::TextWrapped("%s", str.c_str());

        ImGui::PopID();
        i++;
    }

    ImGui::EndTable();
}

/**
 * Draws the globule list.
 */
void WorldChunkDebugger::drawGlobules() {
   ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 10);
    if(!ImGui::BeginTable("globules", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 58);
    ImGui::TableSetupColumn("Vertices");
    ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 28);
    ImGui::TableHeadersRow();

    size_t i = 0;
    for(const auto &[key, globule] : this->chunk->globules) {
        ImGui::TableNextRow();
        ImGui::PushID(i);

        ImGui::TableNextColumn();
        ImGui::Text("%d,%d,%d", (int) globule->position.x, (int) globule->position.y, 
                (int) globule->position.z);

        ImGui::TableNextColumn();
        ImGui::Text("%lu", globule->vertexData.size());

        ImGui::TableNextColumn();
        ImGui::Checkbox("##visible", &globule->isVisible);

        ImGui::PopID();
        i++;
    }

    ImGui::EndTable();
}
