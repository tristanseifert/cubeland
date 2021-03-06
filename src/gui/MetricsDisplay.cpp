#include "MetricsDisplay.h"
#include "GameUI.h"

#include "gui/MenuBarHandler.h"

#include <imgui.h>
#include <metricsgui/metrics_gui.h>

using namespace gui;

/**
 * Sets up the metrics display controller.
 */
MetricsDisplay::MetricsDisplay() {
    // FPS
    this->mFrameTime = new MetricsGuiMetric("Frame Time", "s", MetricsGuiMetric::USE_SI_UNIT_PREFIX);
    this->mFrameTime->mSelected = true;

    // set up the actual drawing stuff
    this->gOverlay = new MetricsGuiPlot();
    this->gOverlay->mShowAverage = true;
    this->gOverlay->mShowLegendAverage = true;

    this->gOverlay->AddMetric(this->mFrameTime);

    this->gList = new MetricsGuiPlot();
    this->gList->mShowInlineGraphs = true;
    this->gList->mInlinePlotRowCount = 3;

    this->gList->AddMetric(this->mFrameTime);

    this->fpsOverlayMenuItem = gui::MenuBarHandler::registerItem("Overlays", "Frame Times", &this->showFpsOverlay);
}
/**
 * Cleans up the metrics display.
 */
MetricsDisplay::~MetricsDisplay() {
    delete this->gOverlay;
    delete this->gList;

    delete this->mFrameTime;

    gui::MenuBarHandler::unregisterItem(this->fpsOverlayMenuItem);
}

/**
 * Draws the metrics display.
 *
 * This will draw both the FPS overlay and the general list.
 */
void MetricsDisplay::draw(GameUI *ui) {
    // main window
    if(this->showMetrics) {
        this->gList->UpdateAxes();
        this->drawMetricsWindow(ui);
    }

    // draw FPS overlay
    if(this->showFpsOverlay) {
        this->gOverlay->UpdateAxes();
        this->drawOverlay(ui);
    }
}

/**
 * Draw the main metrics window.
 */
void MetricsDisplay::drawMetricsWindow(GameUI *ui) {
    // start window
    if(!ImGui::Begin("Señor Metrics", &this->showMetrics)) {
        return;
    }

    // checkbox to toggle the overlay
    ImGui::Checkbox("Show Overlay", &this->showFpsOverlay);

    // draw the metrics list
    this->gList->DrawList();

    // finish
    ImGui::End();
}

/**
 * Draws the FPS overlay.
 */
void MetricsDisplay::drawOverlay(GameUI *ui) {
    // distance from the edge of display for the overview
    const float DISTANCE = 10.0f;
    const size_t corner = 0; // top-left
    ImGuiIO& io = ImGui::GetIO();

    // get the window position
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
    ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowSize(ImVec2(400, 0));
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

    ImGui::SetNextWindowBgAlpha(kOverlayAlpha);
    if(!ImGui::Begin("FPS Overlay", &this->showFpsOverlay, window_flags)) {
        return;
    }

    // draw the average FPS label
    ImGui::Text("FPS: %g", this->fps);

    // draw the metric
    if(this->showOverlayGraph) {
        this->gOverlay->DrawHistory();
    }

    // context menu
    if(ImGui::BeginPopupContextWindow()) {
        ImGui::MenuItem("Show Metrics List", nullptr, &this->showMetrics);

        ImGui::Separator();
        ImGui::MenuItem("Show Frame Time Graph", nullptr, &this->showOverlayGraph);
        if(this->showFpsOverlay && ImGui::MenuItem("Close Overlay")) {
            this->showFpsOverlay = false;
        }

        ImGui::EndPopup();
    }

    // finish
    ImGui::End();
}
