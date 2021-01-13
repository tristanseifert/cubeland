/**
 * Displays the about window, including licenses and third party acknowledgements.
 */
#ifndef GUI_TITLE_ABOUTWINDOW_H
#define GUI_TITLE_ABOUTWINDOW_H

#include "gui/GameWindow.h"

#include <string>
#include <memory>

namespace ImGui {
struct MarkdownFormatInfo;
}

namespace gui {
class GameUI;
}

namespace gui::title {
class AboutWindow: public gui::GameWindow {
    public:
        AboutWindow();
        virtual ~AboutWindow() = default;

        void draw(gui::GameUI *) override;

    private:
        static void markdownFormat(const ImGui::MarkdownFormatInfo &, bool);
        void markdown(gui::GameUI *, const std::string &);

    private:
        // loaded markdown strings
        std::string mdAbout, mdLicenses;
};
}

#endif
