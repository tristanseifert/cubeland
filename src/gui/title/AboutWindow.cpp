#include "AboutWindow.h"
#include "gui/GameUI.h"

#include "io/ResourceManager.h"

#include <version.h>

#include <imgui.h>
#include <imgui_markdown.h>

using namespace gui;
using namespace gui::title;

/**
 * Initializes the about window; the markdown text to be displayed is loaded.
 */
AboutWindow::AboutWindow() {
    std::vector<unsigned char> temp;

    // load about
    io::ResourceManager::get("text/about.md", temp);
    this->mdAbout = std::string(temp.begin(), temp.end());

    // load licenses
    io::ResourceManager::get("text/third_party.md", temp);
    this->mdLicenses = std::string(temp.begin(), temp.end());
}



/**
 * Draws the about window.
 */
void AboutWindow::draw(GameUI *gui) {
    // begin the window
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    ImGui::SetNextWindowFocus();
    ImGui::SetNextWindowSize(ImVec2(800, 600));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    if(!ImGui::Begin("About Cubeland", &this->visible, winFlags)) {
        return ImGui::End();
    }

    // create a tab bar
   if(ImGui::BeginTabBar("aboutMain")) {
        if(ImGui::BeginTabItem("License")) {
            if(ImGui::BeginChild("licenses", ImVec2(0, -0))) {
                this->markdown(gui, this->mdAbout);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Acknowledgements")) {
            if(ImGui::BeginChild("acknowledge", ImVec2(0, -0))) {
                this->markdown(gui, this->mdLicenses);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // version info
        if(ImGui::BeginTabItem("Build")) {
            auto mono = gui->getFont(GameUI::kGameFontMonospaced);

            ImGui::Bullet();
            ImGui::Text("Version: ");
            ImGui::SameLine();
            ImGui::PushFont(mono);
            ImGui::TextUnformatted(gVERSION);
            ImGui::PopFont();

            ImGui::Bullet();
            ImGui::Text("Build: ");
            ImGui::SameLine();
            ImGui::PushFont(mono);
            ImGui::TextUnformatted(gVERSION_HASH);
            ImGui::PopFont();

            ImGui::Bullet();
#ifdef NDEBUG
            ImGui::Text("Build Type: Release");
#else
            ImGui::Text("Build Type: Debug");
#endif

            ImGui::Bullet();
            ImGui::Text("Built On: ");
            ImGui::SameLine();
            ImGui::Text("%s at %s", __DATE__, __TIME__);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // clean up
    ImGui::End();
}



/**
 * Markdown format callback; this applies the proper fonts for bold/italic.
 */
void AboutWindow::markdownFormat(const ImGui::MarkdownFormatInfo &info, bool start) {
    // get the game ui ptr
    auto gui = reinterpret_cast<GameUI *>(info.config->userData);

    // handle the types of formats
    switch(info.type) {
        case ImGui::MarkdownFormatType::NORMAL_TEXT:
            if(start) {
                ImGui::PushFont(gui->getFont(GameUI::kGameFontBodyRegular));
            } else {
                ImGui::PopFont();
            }
            break;

        default:
            ImGui::defaultMarkdownFormatCallback(info, start);
            break;
    }
}

/**
 * Markdown rendering helper
 */
void AboutWindow::markdown(GameUI *gui, const std::string &str) {
    const ImGui::MarkdownConfig mdConfig {
        // link callback
        nullptr, 
        // tooltip callback
        nullptr,
        // image callback
        nullptr,
        // link icon
        ">",
        // heading formats
        {
            {gui->getFont(GameUI::kGameFontHeading), true},
            {gui->getFont(GameUI::kGameFontHeading2), true},
            {gui->getFont(GameUI::kGameFontHeading3), false},
        },
        // user data
        gui,
        // format callback
        &AboutWindow::markdownFormat
    };

    ImGui::Markdown(str.c_str(), str.length(), mdConfig);
}
