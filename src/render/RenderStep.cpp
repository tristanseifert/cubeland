#include "RenderStep.h"

#include "gui/MenuBarHandler.h"

using namespace render;

/**
 * Sets up a render step, registering the debug window flag as a menu item.
 */
RenderStep::RenderStep(const std::string &category, const std::string &title) {
    this->debugMenuItem = gui::MenuBarHandler::registerItem(category, title, &this->showDebugWindow);
}

/**
 * Deregisters the menu item if required.
 */
RenderStep::~RenderStep() {
    if(this->debugMenuItem) {
        gui::MenuBarHandler::unregisterItem(this->debugMenuItem);
    }
}
