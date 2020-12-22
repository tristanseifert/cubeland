#include "MetricsManager.h"

#include "gui/MetricsDisplay.h"

#include <metricsgui/metrics_gui.h>

using namespace io;
using namespace gui;

std::shared_ptr<gui::MetricsDisplay> MetricsManager::gDisplay = nullptr;

void MetricsManager::submitFrameTime(float time) {
    gDisplay->mFrameTime->AddNewValue(time);
}

void MetricsManager::submitChunkMetrics(size_t numData, size_t numDisplay, size_t numCulled) {
    gDisplay->mDataChunks->AddNewValue(numData);
    gDisplay->mDisplayChunks->AddNewValue(numDisplay);
    gDisplay->mDisplayCulled->AddNewValue(numCulled);
}

