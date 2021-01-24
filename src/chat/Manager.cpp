#include "Manager.h"
#include "Window.h"

#include "gui/GameUI.h"
#include "input/InputManager.h"
#include "world/ClientWorldSource.h"
#include "world/RemoteSource.h"
#include "net/ServerConnection.h"
#include "net/handlers/Chat.h"

#include <io/Format.h>
#include <Logging.h>

#include <SDL.h>

#include <stdexcept>

using namespace chat;



/**
 * Initializes the chat manager.
 */
Manager::Manager(input::InputManager *_mgr, std::shared_ptr<gui::GameUI> &_gui, 
        std::shared_ptr<world::ClientWorldSource> &source) : in(_mgr), gui(_gui), world(source) {
    auto remote = std::dynamic_pointer_cast<world::RemoteSource>(source);
    if(!remote) {
        throw std::runtime_error("Chat only supported for remote sources!");
    }

    auto server = remote->getServer();
    this->server = server;

    // set up UI
    this->ui = std::make_shared<Window>(this);
    this->gui->addWindow(this->ui);

    // register for chat event handlers
    this->chatEventToken = server->chat->addCallback(std::bind(&Manager::chatEvent, this,
                std::placeholders::_1));

    // insert info for all existing players
    auto fut = server->getConnectedPlayers();
    auto players = fut.get();

    for(const auto &player : players) {
        this->ui->setPlayerInfo(player.id, player.displayName);
    }
}

/**
 * Shuts down the chat manager.
 */
Manager::~Manager() {
    if(this->chatEventToken) {
        auto server = this->server.lock();
        server->chat->removeCallback(this->chatEventToken);
    }

    this->gui->removeWindow(this->ui);
}


/**
 * Handles an SDL event. This is roughly divided into two states:
 *
 * - Detailed view not open: The "T" key will open the chat view.
 * - Detailed view open: The ESC key will close the detailed view.
 */
bool Manager::handleEvent(const SDL_Event &event) {
    // ignore anything that's not a key down event
    if(event.type != SDL_KEYDOWN) return false;

    const auto &k = event.key.keysym;

    if(this->ui->isChatOpen()) {
        if(k.scancode == SDL_SCANCODE_ESCAPE && this->in->getCursorCount() != 0) {
            this->in->decrementCursorCount();
            this->ui->setChatOpen(false);
            return true;
        }
    } else if(!this->in->getCursorCount()) {
        // T opens chat
        if(k.scancode == SDL_SCANCODE_T) {
            this->in->incrementCursorCount();
            this->ui->setChatOpen(true);
            return true;
        }
    }

    // event not handled
    return false;
}


/**
 * Chat event callback
 */
void Manager::chatEvent(const net::handler::Chat::EventInfo &info) {
    using namespace net::handler;

    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        // send messages to all clients
        if constexpr (std::is_same_v<T, Chat::Message>) {
            // Logging::trace("Message from {}: {}", arg.from, arg.message);
            this->ui->rxMessage(arg.from, arg.message);
        }
        // a player joined the server
        else if constexpr (std::is_same_v<T, Chat::PlayerJoined>) {
            this->ui->setPlayerInfo(arg.id, arg.name);
            this->ui->playerJoined(arg.id);
        }
        // a player left the server
        else if constexpr (std::is_same_v<T, Chat::PlayerLeft>) {
            this->ui->playerLeft(arg.id);
        }
        // everything else is treated as a no-op
        else {}
    }, info);
}

/**
 * Yeets a message to all other clients.
 */
void Manager::sendMessage(const std::string &msg) {
    auto server = this->server.lock();
    server->chat->sendMessage(msg);
}
