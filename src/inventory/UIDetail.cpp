#include "UIDetail.h"

#include <imgui.h>

using namespace inventory;

/**
 * Draws the main window of the detail view.
 *
 * Note that this is called from within BeginPopupModal() context already, so there is no need to
 * create (or end) the window to hold the view.
 */
void UIDetail::draw(gui::GameUI *gui) {
    ImGui::Text("fucker dot com");
}

