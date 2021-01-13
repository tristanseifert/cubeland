#ifndef GUI_GAMEWINDOW_H
#define GUI_GAMEWINDOW_H

namespace gui {
class GameUI;

class GameWindow {
    public:
        /// Draw the controls desired here
        virtual void draw(GameUI *gui) = 0;
        /// Whether the window is currently visible or not
        virtual bool isVisible() const {
            return this->visible;
        }
        /// Sets the visibility state of the window
        virtual void setVisible(const bool newVisible) {
            this->visible = newVisible;
        }

        /// Whether this window uses the game's UI style or the default debug style
        virtual bool usesGameStyle() const {
            return true;
        }

    protected:
        bool visible = true;
};
};

#endif
