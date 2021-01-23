#ifndef GUI_DISCONNECTEDERROR_H
#define GUI_DISCONNECTEDERROR_H

#include "GameWindow.h"
#include "GameUI.h"

#include <memory>
#include <optional>
#include <string>

#include <imgui.h>

namespace gui {
class GameUI;

class DisconnectedError: public gui::GameWindow {
    public:
        DisconnectedError(const std::optional<std::string> &_msg) : msg(_msg) {};
        DisconnectedError(const std::optional<std::string> &_msg, std::weak_ptr<DisconnectedError> &_self) : 
            msg(_msg), self(_self) {};
        virtual ~DisconnectedError() = default;

        void setSelf(std::weak_ptr<DisconnectedError> newSelf) {
            this->self = newSelf;
        }

    public:
        void draw(GameUI *gui) override {
            // constrain prefs window size
            ImGuiIO& io = ImGui::GetIO();

            ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
            ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

            ImGui::SetNextWindowFocus();
            ImGui::SetNextWindowSizeConstraints(ImVec2(600, 320), ImVec2(600, 475));
            ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

            // short circuit drawing if not visible
            if(!ImGui::Begin("Disconnected", &this->visible, winFlags)) {
                return ImGui::End();
            }

            ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
            ImGui::TextWrapped("An unexpected error caused the network connection to be disconnected.");
            ImGui::PopFont();

            ImGui::TextWrapped("Your most recent changes may not have been recorded by the server. Check your network connection and try reconnecting; if the issue persists, contact the server owner.");

            // detail
            if(this->msg) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
                if(ImGui::CollapsingHeader("Details")) {
                    ImGui::TextWrapped("%s", this->msg->c_str());
                    ImGui::Dummy(ImVec2(0,4));
                }
            }

            // buttons
            const float spaceV = ImGui::GetContentRegionAvail().y;
            ImGui::Dummy(ImVec2(0, spaceV - 22 - 8 - 6));

            ImGui::Separator();

            if(ImGui::Button("Close")) {
                auto self = this->self.lock();
                if(self) {
                    gui->removeWindow(self);
                }
            }

            // begin
            ImGui::End();
        }

    private:
        /// optional error detail
        std::optional<std::string> msg;

        /// used to remove ourselves
        std::weak_ptr<DisconnectedError> self;
};
}

#endif
