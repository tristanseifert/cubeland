#include "WorldRendererDebugger.h"
#include "WorldRenderer.h"
#include "render/RenderStep.h"

#include <imgui.h>

using namespace render;

/**
 * Sets up a world renderer debugger.
 */
WorldRendererDebugger::WorldRendererDebugger(WorldRenderer *_renderer) : renderer(_renderer) {

}

/**
 * Draws the world renderer debug window.
 */
void WorldRendererDebugger::draw() {
    if(!ImGui::Begin("World Renderer", &this->isDebuggerOpen)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Time:");
    ImGui::SameLine();
    ImGui::Text("%g", this->renderer->getTime());

    ImGui::Checkbox("Paused", &this->renderer->paused);
    if(this->renderer->paused) {
        float time = this->renderer->time;
        if(ImGui::DragFloat("Time", &time, 0.001, 0)) {
            this->renderer->time = time;
        }
    }

    if(ImGui::CollapsingHeader("View")) {
        this->drawFovUi();
    }

    if(ImGui::CollapsingHeader("Steps")) {
        this->drawStepsTable();
    }

    ImGui::End();
}

/**
 * Controls to adjust FoV/Z clipping
 */
void WorldRendererDebugger::drawFovUi() {
    ImGui::PushItemWidth(74);

    ImGui::DragFloat("FoV (°)", &this->renderer->projFoV, 1, 30, 120);

    ImGui::DragFloat("ZNear", &this->renderer->zNear, 0.001, 0.0000001);
    ImGui::DragFloat("ZFar", &this->renderer->zFar, 0.001, 0.0000001);

    ImGui::PopItemWidth();
}

/**
 * Draws a table listing all render steps.
 */
void WorldRendererDebugger::drawStepsTable() {
    ImVec2 outerSize(0, ImGui::GetTextLineHeightWithSpacing() * 6);
    if(!ImGui::BeginTable("steps", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ColumnsWidthStretch | ImGuiTableFlags_ScrollY, outerSize)) {
        return;
    }

    ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 15);
    ImGui::TableSetupColumn("Impl");
    ImGui::TableSetupColumn("Debug", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableHeadersRow();

    size_t i = 0;
    for(const auto info : this->renderer->steps) {
        ImGui::TableNextRow();
        ImGui::PushID(i);

        ImGui::TableNextColumn();
        ImGui::Text("%lu", (i+1));
        ImGui::TableNextColumn();
        ImGui::Text("%p", info.get());

        ImGui::TableNextColumn();
        ImGui::Checkbox("##debug", &info->showDebugWindow);

        ImGui::PopID();
        i++;
    }

    ImGui::EndTable();
}
