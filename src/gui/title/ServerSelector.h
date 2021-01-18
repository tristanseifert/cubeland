#ifndef GUI_TITLE_SERVERSELECTOR_H
#define GUI_TITLE_SERVERSELECTOR_H

#include "gui/GameWindow.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <blockingconcurrentqueue.h>

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

        /// API requests to be made from worker thread
        enum class PlainRequest {
            /// attempt to register authentication key
            RegisterKey,
        };

        using WorkItem = std::variant<std::monostate, PlainRequest>;

    private:
        void saveRecents();

        void drawAccountBar(GameUI *);
        void drawServerList(GameUI *);
        void drawAddServerModal(GameUI *);

        void drawKeypairGenaratorModal(GameUI *);

        void workerMain();
        void workerRegisterKey();

    private:
        /// title screen that provides our background
        TitleScreen *title = nullptr;

        std::unique_ptr<std::thread> worker = nullptr;
        std::atomic_bool workerRun;
        moodycamel::BlockingConcurrentQueue<WorkItem> work;

        /// number of assertions on window focus; if 0, no other windows are open
        size_t focusLayers = 0;
        /// when set, the "generate keypair" dialog is shown
        bool needsKeypairGen = false;
        /// when set, the "add server" dialog is shown
        bool showAddServer = false;

        /// the "register key" modal can be closed
        std::atomic_int closeRegisterModal = 0;
        /// detail to show in the registration error dialog
        std::optional<std::string> registerErrorDetail;

        /// list of connected servers (and some other info)
        Recents recents;
        /// index into the recents list for selection
        int selectedServer = -1;
        /// whether the loading indicator is displayed in the main UI
        bool showLoader = false;

        /// URL/address of the server to add
        std::array<char, 256> addServerUrl;
};
}

#endif
