#ifndef GUI_PREFERENCESWINDOW_H
#define GUI_PREFERENCESWINDOW_H

#include "GameWindow.h"

#include <array>
#include <functional>
#include <string>

namespace gui {
class GameUI;
class MainWindow;

class PreferencesWindow: public GameWindow {
    public:
        PreferencesWindow(MainWindow *window);
        virtual ~PreferencesWindow() = default;

        void load() {
            this->loadUiPaneState();
            this->loadGfxPaneState();
            this->loadPerfPaneState();
        }

    public:
        void draw(GameUI *) override;

    private:
        void loadUiPaneState();
        void saveUiPaneState();
        void drawUiPane(GameUI *);

        void loadGfxPaneState();
        void saveGfxPaneState();
        void drawGfxPane(GameUI *);
        void loadGfxPresetLow() {
            this->gfx.fancySky = false;
            this->gfx.dirShadows = false;
            this->gfx.ssao = false;
        }
        void loadGfxPresetMedium() {
            this->gfx.fancySky = true;
            // this->gfx.dirShadows = true;
            this->gfx.ssao = false;
        }
        void loadGfxPresetHigh() {
            this->gfx.fancySky = true;
            // this->gfx.dirShadows = true;
            this->gfx.ssao = true;
        }
        void loadGfxPresetUltra() {
            // TODO: make this its own thing
            loadGfxPresetHigh();
        }

        void loadPerfPaneState();
        void savePerfPaneState();
        void drawPerfPane(GameUI *);

        void drawKeyValue(GameUI *, const std::string &key, const std::string &value);

    private:
        const std::array<std::function<void()>, 4> kGfxPresets = {
            std::bind(&PreferencesWindow::loadGfxPresetLow, this),
            std::bind(&PreferencesWindow::loadGfxPresetMedium, this),
            std::bind(&PreferencesWindow::loadGfxPresetHigh, this),
            std::bind(&PreferencesWindow::loadGfxPresetUltra, this),
        };

    private:
        // main window (so we can force GUI updates)
        gui::MainWindow *window = nullptr;

        // state for the UI prefs pane
        struct {
            bool restoreWindowSize;
            bool dpiAware;
            bool vsync;
        } stateUi;

        // state for the graphics prefs pane
        struct {
            // index of preset to use when resetting gfx settings
            size_t preset = 0;

            bool fancySky;
            bool dirShadows;
            bool ssao;

            float gamma;
            // field of view (degrees)
            float fov;
            // horizontal inventory-ness
            bool horizontalInventory;
        } gfx;

        // state for the performance pane
        struct {
            // drawing worker threads
            int drawThreads = 0;
            // world source threads
            int sourceThreads = 0;

            // render distance (in chunks)
            int renderDist = 0;
            // how many chunks outside render distance to keep in cache
            int renderCacheBuffer = 0;
        } perf;
};
}

#endif
