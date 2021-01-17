#ifndef GUI_TITLE_SERVERSELECTOR_H
#define GUI_TITLE_SERVERSELECTOR_H

#include "gui/GameWindow.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace gui {
class TitleScreen;
}

namespace gui::title {
class ServerSelector: public gui::GameWindow {
    public:
        ServerSelector(TitleScreen *title);
        virtual ~ServerSelector();

        void draw(GameUI *) override;

        void clear();
        void loadRecents();

    private:
        static const std::string kPrefsKey;

        /// Entry in the recents list
        struct Server {
            /// server address (IP address or DNS name)
            std::string address;
            /// last opened timestamp
            std::chrono::system_clock::time_point lastConnected;

            /// Creates a new recents entry with the current time.
            Server(const std::string &_addrStr) : address(_addrStr) {
                this->lastConnected = std::chrono::system_clock::now();
            }
            /// Creates an uninitialized recents entry
            Server() = default;

            template<class Archive> void serialize(Archive &arc) {
                arc(this->address);
                arc(this->lastConnected);
            }
        };

        /// List of servers we've connected to. We call it recents since that's what it's ordered by
        struct Recents {
            /// servers we've connected to
            std::vector<Server> servers;

            /// last time we checked in with the web service
            std::chrono::system_clock::time_point lastApiCheckin;

            /// Whether any servers are in the recents list
            const bool empty() const {
                return this->servers.empty();
            }

            /// sorts the list of servers in descending connection time
            void sort() {
                std::sort(std::begin(this->servers), std::end(this->servers), 
                        [](const auto &l, const auto &r) {
                    return l.lastConnected > r.lastConnected;
                });
            }

            template<class Archive> void serialize(Archive &arc) {
                arc(this->servers);
                arc(this->lastApiCheckin);
            }
        };

    private:
        void saveRecents();

        void drawAccountBar(GameUI *);
        void drawServerList(GameUI *);

        void drawKeypairGenaratorModal(GameUI *);

    private:
        /// title screen that provides our background
        TitleScreen *title = nullptr;

        /// number of assertions on window focus; if 0, no other windows are open
        size_t focusLayers = 0;
        /// when set, the "generate keypair" dialog is shown
        bool needsKeypairGen = false;

        /// list of connected servers (and some other info)
        Recents recents;
        /// index into the recents list for selection
        int selectedServer = -1;
        /// whether the loading indicator is displayed in the main UI
        bool showLoader = false;
};
}

#endif
