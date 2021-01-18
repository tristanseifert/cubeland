#include "MetricsManager.h"

#include "gui/MetricsDisplay.h"

#include <metricsgui/metrics_gui.h>

using namespace io;
using namespace gui;

std::shared_ptr<gui::MetricsDisplay> MetricsManager::gDisplay = nullptr;

void MetricsManager::submitFrameTime(const float time) {
    gDisplay->mFrameTime->AddNewValue(time);
}

void MetricsManager::setFps(const float fps) {
    gDisplay->fps = fps;
}
