/**
 * Draws a preferences window that allows editing of a subset of options in game.
 */
#ifndef GUI_INGAMEPREFSWINDOW_H
#define GUI_INGAMEPREFSWINDOW_H

#include "GameWindow.h"

namespace render {
class WorldRenderer;
}

namespace gui {
class InGamePrefsWindow: public GameWindow {
    public:
        InGamePrefsWindow(render::WorldRenderer *_renderer) : renderer(_renderer) {
            this->load();
        };
        virtual ~InGamePrefsWindow() = default;

        void load();
        void save();

        void draw(GameUI *) override;

    private:
        render::WorldRenderer *renderer = nullptr;

        float fov;
        int renderDist;
};
}

#endif
