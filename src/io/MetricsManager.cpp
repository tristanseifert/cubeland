#include "MetricsManager.h"

#include "gui/MetricsDisplay.h"

#include <metricsgui/metrics_gui.h>

using namespace io;
using namespace gui;

std::shared_ptr<gui::MetricsDisplay> MetricsManager::gDisplay = nullptr;

void MetricsManager::submitFrameTime(float time) {
    gDisplay->mFrameTime->AddNewValue(time);
}


