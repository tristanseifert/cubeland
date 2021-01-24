#ifndef CHAT_MANAGER_H
#define CHAT_MANAGER_H

#include "net/handlers/Chat.h"

#include <memory>
#include <string>

union SDL_Event;

namespace world {
class ClientWorldSource;
}
namespace input {
class InputManager;
}
namespace net {
class ServerConnection;
}
namespace gui {
class GameUI;
}

namespace chat {
class Window;

class Manager {
    friend class Window;

    public:
        Manager(input::InputManager *in, std::shared_ptr<gui::GameUI> &gui, std::shared_ptr<world::ClientWorldSource> &_src);
        ~Manager();

        bool handleEvent(const SDL_Event &);

    private:
        void chatEvent(const net::handler::Chat::EventInfo &);

        void sendMessage(const std::string &);

    private:
        input::InputManager *in = nullptr;

        std::shared_ptr<world::ClientWorldSource> world = nullptr;
        std::weak_ptr<net::ServerConnection> server;

        uint32_t chatEventToken = 0;

        std::shared_ptr<gui::GameUI> gui = nullptr;
        std::shared_ptr<Window> ui = nullptr;
};
}

#endif
