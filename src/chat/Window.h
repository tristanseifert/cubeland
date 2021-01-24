#ifndef CHAT_WINDOW_H
#define CHAT_WINDOW_H

#include "gui/GameWindow.h"

#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

#include <uuid.h>

namespace chat {
class Manager;

class Window: public gui::GameWindow {
    friend class Manager;

    public:
        Window(Manager *_owner);
        virtual ~Window() = default;

        void draw(gui::GameUI *gui) override;

        // always visible (to allow drawing the notifications)
        bool isVisible() const override {
            return true;
        }
        // whether chat window is open
        bool isChatOpen() const {
            return this->chatOpen;
        }
        // set chat open status
        void setChatOpen(const bool open) {
            this->chatOpen = open;
            if(open) {
                this->chatFirstAppearance = true;
            }
        }

    private:
        struct Entry {
            std::string text;
            bool italic = false;

            Entry() = default;
            Entry(const std::string &_text) : text(_text) {}
            Entry(const std::string &_text, const bool _italic) : text(_text), italic(_italic) {}
        };

        struct PlayerInfo {
            std::string displayName;

            PlayerInfo() = default;
            PlayerInfo(const std::string &_name) : displayName(_name) {}
        };

    private:
        void drawChatWindow(gui::GameUI *);

        void handleInput(const std::string &);

        void setPlayerInfo(const uuids::uuid &uuid, const std::string &name) {
            std::lock_guard<std::mutex> lg(this->infoLock);
            this->info[uuid] = PlayerInfo(name);
        }

        void rxMessage(const std::optional<uuids::uuid> &, const std::string &);
        void playerJoined(const uuids::uuid &);
        void playerLeft(const uuids::uuid &);

    private:
        /// transparency of the window
        constexpr static const float kWindowBgAlpha = .9;

        constexpr static const size_t kSendMsgBufLen = 2048;

    private:
        bool chatOpen = false, chatFirstAppearance = true;
        int focusLayers = 0;

        Manager *manager = nullptr;

        std::array<char, kSendMsgBufLen> sendMsgBuf;

        std::mutex scrollbackLock;
        std::vector<Entry> scrollback;

        std::mutex infoLock;
        std::unordered_map<uuids::uuid, PlayerInfo> info;
};
}

#endif
