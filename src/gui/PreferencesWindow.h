#ifndef GUI_PREFERENCESWINDOW_H
#define GUI_PREFERENCESWINDOW_H

#include "GameWindow.h"

#include <string>

namespace gui {
class GameUI;

class PreferencesWindow: public GameWindow {
    public:
        PreferencesWindow();

    public:
        void draw(GameUI *) override;

    private:
        void loadUiPaneState();
        void saveUiPaneState();
        void drawUiPane(GameUI *);

        void drawKeyValue(GameUI *, const std::string &key, const std::string &value);

    private:
        // state for the UI prefs pane
        struct {
            bool restoreWindowSize;
            bool dpiAware;
        } stateUi;
};
}

#endif
