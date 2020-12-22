#ifndef IO_METRICSMANAGER_H
#define IO_METRICSMANAGER_H

#include <memory>

namespace gui {
class MetricsDisplay;
}

namespace io {
class MetricsManager {
    private:
        MetricsManager();

    public:
        /// Sets the display used for metrics.
        static void setDisplay(std::shared_ptr<gui::MetricsDisplay> disp) {
            gDisplay = disp;
        }

        /// Submits the given frame time to the metrics display.
        static void submitFrameTime(float time);

    private:
        static std::shared_ptr<gui::MetricsDisplay> gDisplay;
};
}

#endif
