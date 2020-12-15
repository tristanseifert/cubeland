/**
 * Provides a graphical debugger for working with the world reader system.
 *
 * As the game works, this debugger would get a reference to whatever world is being displayed on
 * the screen at the time.
 */
#ifndef WORLD_DEBUG_WORLDDEBUGGER_H
#define WORLD_DEBUG_WORLDDEBUGGER_H

#include "gui/GameWindow.h"

#include <string>
#include <memory>

namespace gui {
class GameUI;
}

namespace world {
class WorldReader;

class WorldDebugger: public gui::GameWindow {
    public:
        void draw(gui::GameUI *) override;

        /// Returns the visibility state of the debugger.
        bool isOpen() const {
            return this->isDebuggerOpen;
        }
        /// Sets the visibility state of the debugger.
        void setOpen(bool open) {
            this->isDebuggerOpen = open;
        }
        /// Sets the world displayed in the debugger.
        void setWorld(std::shared_ptr<WorldReader> newWorld) {
            this->world = newWorld;
        }

    private:
        void loadWorldInfo();

    private:
        /// Whether the debug window is open
        bool isDebuggerOpen = true;

        /// World reader currently in use
        std::shared_ptr<WorldReader> world = nullptr;
        /// Error from opening the world, if any
        std::unique_ptr<std::string> worldError = nullptr;
};
}

#endif
