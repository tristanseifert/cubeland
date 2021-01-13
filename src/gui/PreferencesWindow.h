#ifndef GUI_PREFERENCESWINDOW_H
#define GUI_PREFERENCESWINDOW_H

#include "GameWindow.h"

#include <string>

namespace gui {
class GameUI;

class PreferencesWindow: public GameWindow {
    public:
        PreferencesWindow();
        virtual ~PreferencesWindow() = default;

        void load() {
            this->loadUiPaneState();
            this->loadGfxPaneState();
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

        void drawKeyValue(GameUI *, const std::string &key, const std::string &value);

    private:
        // state for the UI prefs pane
        struct {
            bool restoreWindowSize;
            bool dpiAware;
        } stateUi;

        // state for the graphics prefs pane
        struct {
            // index of preset to use when resetting gfx settings
            size_t preset = 0;

            bool fancySky;
            bool dirShadows;
        } gfx;
};
}

#endif
