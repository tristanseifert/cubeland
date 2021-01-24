#include "Window.h"
#include "Manager.h"

#include "gui/GameUI.h"

#include <io/Format.h>
#include <Logging.h>

#include <imgui.h>

using namespace chat;

/**
 * Initializes the chat window.
 */
Window::Window(Manager *_owner) : manager(_owner) {
    std::fill(this->sendMsgBuf.begin(), this->sendMsgBuf.end(), '\0');
}

/**
 * Draws the components of the chat window.
 */
void Window::draw(gui::GameUI *gui) {
    if(this->chatOpen) {
        this->drawChatWindow(gui);
    }
}

/**
 * Draw chat window.
 */
void Window::drawChatWindow(gui::GameUI *gui) {
    static bool refocus = false;
    static bool scrollToBottom = false;

    // window is glued to the bottom left of the screen
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration;
    ImVec2 windowPos = ImVec2(20, io.DisplaySize.y - 20);

    if(!this->focusLayers) {
        ImGui::SetNextWindowFocus();
    }
    ImGui::SetNextWindowSize(ImVec2(std::max(io.DisplaySize.x / 2., 400.), std::min(600., io.DisplaySize.y - 40.)));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(0, 1));
    ImGui::SetNextWindowBgAlpha(kWindowBgAlpha);

    if(!ImGui::Begin("Chat", &this->chatOpen, winFlags)) {
        refocus = true;
        return ImGui::End();
    }
    if(!refocus) {
        refocus = this->chatFirstAppearance;
        this->chatFirstAppearance = false;
    }

    // scrolling content region
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false);

    if(ImGui::BeginPopupContextWindow()) {
        if(ImGui::Selectable("Clear Scrollback")) {
            std::lock_guard<std::mutex> lg(this->scrollbackLock);
            this->scrollback.clear();
        }
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

    // draw each scrollback entry
    {
        std::lock_guard<std::mutex> lg(this->scrollbackLock);

        ImGui::PushFont(gui->getFont(gui::GameUI::kGameFontBodyRegular));

        for(const auto &entry : this->scrollback) {
            if(entry.italic) {
                ImGui::PushFont(gui->getFont(gui::GameUI::kGameFontBodyItalic));
            }

            ImGui::TextWrapped("%s", entry.text.c_str());

            if(entry.italic) {
                ImGui::PopFont();
            }
        }

        ImGui::PopFont();
    }

    if(scrollToBottom) {
        ImGui::SetScrollHereY(1.);
        scrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    // entry zone
    ImGui::Separator();
    ImGui::PushItemWidth(-FLT_MIN);

    if(ImGui::InputText("##message", this->sendMsgBuf.data(), this->sendMsgBuf.size(), 
                ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_EnterReturnsTrue)) {
    /*if(ImGui::InputTextMultiline("##message", this->sendMsgBuf.data(), this->sendMsgBuf.size(), 
                ImVec2(-FLT_MIN, textHeight), ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_EnterReturnsTrue)) {*/
        const auto len = strnlen(this->sendMsgBuf.data(), this->sendMsgBuf.size());
        const std::string msgStr(this->sendMsgBuf.data(), len);

        this->handleInput(msgStr);
        std::fill(this->sendMsgBuf.begin(), this->sendMsgBuf.end(), '\0');
        refocus = true;
        scrollToBottom = true;
    }
    ImGui::SetItemDefaultFocus();
    if(refocus) {
        ImGui::SetKeyboardFocusHere(-1);
        refocus = false;
    }

    // finish
    ImGui::PopItemWidth();
    ImGui::End();
}

/**
 * Handles an entered message.
 */
void Window::handleInput(const std::string &msg) {
    this->manager->sendMessage(msg);
}

/**
 * Yeets a received message into the scrollback.
 */
void Window::rxMessage(const std::optional<uuids::uuid> &id, const std::string &msg) {
    // format string
    std::string string;

    if(id) {
        std::lock_guard<std::mutex> lg(this->infoLock);

        if(this->info.contains(*id)) {
            const auto &info = this->info.at(*id);
            string = f("<{}> {}", info.displayName, msg);
        } else {
            string = f("<{}> {}", *id, msg);
        }
    } else {
        string = f("Global message: {}", msg);
    }

    // yeet it into the buffer
    std::lock_guard<std::mutex> lg(this->scrollbackLock);
    this->scrollback.push_back(Entry(string));
}

/**
 * Handles a "player joined" message
 */
void Window::playerJoined(const uuids::uuid &id) {
    std::lock_guard<std::mutex> lg(this->infoLock);
    const auto &info = this->info.at(id);
    const auto string = f("▶ {} joined", info.displayName);

    // yeet it into the buffer
    std::lock_guard<std::mutex> lg2(this->scrollbackLock);
    this->scrollback.push_back(Entry(string, true));
}

/**
 * Handles a "player left" message
 */
void Window::playerLeft(const uuids::uuid &id) {
    std::lock_guard<std::mutex> lg(this->infoLock);
    const auto &info = this->info.at(id);
    const auto string = f("▶ {} disconnected", info.displayName);

    // yeet it into the buffer
    std::lock_guard<std::mutex> lg2(this->scrollbackLock);
    this->scrollback.push_back(Entry(string, true));
}
