#ifndef GUI_GAMEWINDOW_H
#define GUI_GAMEWINDOW_H

namespace gui {
class GameUI;

class GameWindow {
    public:
        // Draw the controls desired here
        virtual void draw(GameUI *gui) = 0;
        // Whether the window is currently visible or not
        virtual bool isVisible() const {
            return this->visible;
        }

    protected:
        bool visible = true;
};
};

#endif
