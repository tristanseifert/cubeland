#ifndef GUI_METRICSDISPLAY_H
#define GUI_METRICSDISPLAY_H

#include <cstdint>

#include "GameWindow.h"

struct MetricsGuiMetric;
struct MetricsGuiPlot;

namespace io {
class MetricsManager;
}

namespace gui {
class MetricsDisplay: public GameWindow {
    friend class io::MetricsManager;

    public:
        MetricsDisplay();
        virtual ~MetricsDisplay();

        // Draw the controls desired here
        virtual void draw(GameUI *gui) override;

        virtual bool isVisible() const override {
            return this->showMetrics || this->showFpsOverlay;
        }

        bool usesGameStyle() const override {
            return false;
        } 

    private:
        void drawMetricsWindow(GameUI *ui);
        void drawOverlay(GameUI *ui);

    private:
        /// alpha value of the metrics overlay
        constexpr static const float kOverlayAlpha = 0.74f;

    private:
        // metric and plot for frame times
        MetricsGuiMetric *mFrameTime = nullptr;

        // plot for the overlay
        MetricsGuiPlot *gOverlay = nullptr;

        // plot showed in the overall listing window
        MetricsGuiPlot *gList = nullptr;

    private:
        /// when set, the main metrics window is being drawn
        bool showMetrics = false;
        /// when set, the FPS overlay is being drawn
#ifdef NDEBUG
        bool showFpsOverlay = true;
#else
        bool showFpsOverlay = true;
#endif

        uint32_t fpsOverlayMenuItem = 0;
};
}

#endif
