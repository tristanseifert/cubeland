#include "ServerSelector.h"
#include "TitleScreen.h"
#include "gui/GameUI.h"
#include "gui/Loaders.h"

#include "world/RemoteSource.h"
#include "web/AuthManager.h"
#include "util/Thread.h"
#include "net/ServerConnection.h"
#include "io/PrefsManager.h"
#include "io/Format.h"

#include <Logging.h>

#include <imgui.h>
#include <mutils/time/profiler.h>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/archives/portable_binary.hpp>

using namespace gui::title;
using namespace web;

const std::string ServerSelector::kPrefsKey = "ui.serverSelector.recents";

/**
 * Allocates a new server selector.
 */
ServerSelector::ServerSelector(TitleScreen *_title) : title(_title) {
    // set up worker thread
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(std::bind(&ServerSelector::workerMain, this));

    std::fill(this->displayNameBuf.begin(), this->displayNameBuf.end(), '\0');
    std::fill(this->addServerUrl.begin(), this->addServerUrl.end(), '\0');
}

/**
 * Tears down the server selector resources, like our worker thread.
 */
ServerSelector::~ServerSelector() {
    this->workerRun = false;
    this->work.enqueue(WorkItem());
    this->worker->join();

}


/**
 * Resets the UI state when the dialog is about to be opened.
 */
void ServerSelector::clear() {
    this->focusLayers = 0;
    this->closeRegisterModal = 0;

    this->showAddServer = false;

    // check if keypair must be generated
    if(!AuthManager::areKeysAvailable()) {
        // load the display name
        const auto displayName = io::PrefsManager::getString("auth.displayName", "");
        strncpy(this->displayNameBuf.data(), displayName.c_str(), this->displayNameBuf.size());

        this->needsKeypairGen = true;
        this->focusLayers++;
    } else {
        this->needsKeypairGen = false;
        this->refreshServerStatus();
    }
}

/**
 * Loads the list of recently connected servers.
 */
void ServerSelector::loadRecents() {
    // get the raw recents data blob
    auto blob = io::PrefsManager::getBlob(kPrefsKey);
    if(!blob) return;

    // attempt decoding
    try {
        std::stringstream stream(std::string(blob->begin(), blob->end()));
        cereal::PortableBinaryInputArchive arc(stream);

        Recents r;
        arc(r);

        this->recents = std::move(r);
        this->selectedServer = -1;
    } catch(std::exception &e) {
        Logging::error("Failed to deserialize server recents list: {}", e.what());
    }
}

/**
 * Saves the list of recent servers. It's sorted from most recently connected to least before being
 * saved.
 */
void ServerSelector::saveRecents() {
    // sort recents in newest to oldest order
    this->recents.sort();

    // write recents data
    std::stringstream stream;

    cereal::PortableBinaryOutputArchive arc(stream);
    arc(this->recents);

    const auto str = stream.str();

    std::vector<unsigned char> blobData(str.begin(), str.end());
    io::PrefsManager::setBlob(kPrefsKey, blobData);
}



/**
 * Perform switching to the loaded world if desired.
 */
void ServerSelector::startOfFrame() {
    if(this->wantsOpenWorld) {
        this->title->openWorld(this->connectedWorld);
        this->wantsOpenWorld = false;
    }
}



/**
 * Draws the server selector window.
 *
 * This consists of a list of recently played servers, for each of which we'll try to get some
 * sort of connectivity/status information. There's also a method to get into the account
 * management area.
 *
 * If this view is opened, and we do NOT have a local keypair or player ID saved, we'll prompt
 * the user to generate one.
 */
void ServerSelector::draw(GameUI *gui) {
    // constrain prefs window size
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    if(!this->focusLayers) {
        ImGui::SetNextWindowFocus();
    }
    ImGui::SetNextWindowSize(ImVec2(800, 600));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    // short circuit drawing if not visible
    if(!ImGui::Begin("Join Multi Player World", &this->visible, winFlags)) {
        return ImGui::End();
    }

    // top status bar
    this->drawAccountBar(gui);
    ImGui::Separator();

    // server list
    this->drawServerList(gui);

    // bottom actions
    ImGui::Separator();

    if(ImGui::Button("Add Server...")) {
        ImGui::OpenPopup("Add Server");
        this->focusLayers++;
        this->showAddServer = true;
    } if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add a new server to the list of servers");
    }

    // actions for the selected item
    if(this->selectedServer >= 0) {
        // spacing
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));

        // connect
        ImGui::SameLine();
        if(ImGui::Button("Connect")) {
            auto &server = this->recents.servers[this->selectedServer];
            this->connect(server);
        } if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Join the selected server");
        }

        // remove from recents
        ImGui::SameLine();
        if(ImGui::Button("Remove Selected")) {
            this->recents.servers.erase(this->recents.servers.begin() + this->selectedServer);

            this->selectedServer = -1;
            this->saveRecents();
        } if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Deletes the selected server from the list");
        }
    }

    // various modales
    if(this->needsKeypairGen) {
        ImGui::OpenPopup("Generate Keypair");
        this->drawKeypairGenaratorModal(gui);
    }
    if(this->showManageAccount) {
        this->drawManageAccountModal(gui);
    }
    if(this->showAddServer) {
        this->drawAddServerModal(gui);
    }
    if(this->isConnecting) {
        this->drawConnectingModal(gui);
    }

    // clean up
    ImGui::End();
}

/**
 * Draws the account actions toolbar, as well as the progress indicator, at the top of the window.
 */
void ServerSelector::drawAccountBar(GameUI *gui) {
    if(ImGui::Button("Manage Account")) {
        // load account state
        const auto displayName = io::PrefsManager::getString("auth.displayName", "");
        strncpy(this->displayNameBuf.data(), displayName.c_str(), this->displayNameBuf.size());

        // TODO: show account details
        ImGui::OpenPopup("Manage Account");
        this->showManageAccount = true;
        this->focusLayers++;
    }

    // loading indicator
    if(this->showLoader) {
        ImGui::SameLine();
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2(w - 24 - 8, 0));

        ImGui::SameLine();
        ImGui::Spinner("##spin", 11, 3, ImGui::GetColorU32(ImGuiCol_Button));

        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Making network requests, please wait...");
        }
    }
}

/**
 * Draws the list of servers to which we've recently connected.
 */
void ServerSelector::drawServerList(GameUI *gui) {
    const auto tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_BordersOuter; // | (!this->recents.empty() ? ImGuiTableFlags_Sortable : 0);
    const ImVec2 tableSize(-FLT_MIN, 484);

    if(ImGui::BeginTable("##servers", 2, tableFlags, tableSize)) {
        if(this->recents.empty()) {
            ImGui::TableSetupColumn("##main", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
            ImGui::TextUnformatted("No servers available");
            ImGui::PopFont();
            ImGui::TextWrapped("Click the 'Add Server...' button below to add a server by its address to connect to.");
        } else {
            // configure headers
            ImGui::TableSetupColumn("##main", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 64);

            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            // for each server, list it
            size_t i = 0;
            for(auto &entry : this->recents.servers) {
                // begin a new row
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i);

                std::time_t opened = std::chrono::system_clock::to_time_t(entry.lastConnected);
                const auto openedTm = localtime(&opened);

                char timeBuf[64];
                memset(&timeBuf, 0, sizeof(timeBuf));
                strftime(timeBuf, 63, "%c", openedTm);

                const auto str = f("{}\nLast Connected: {}", entry.address, timeBuf);

                if(ImGui::Selectable(str.c_str(), (this->selectedServer == i), 
                            ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
                    if(i != this->selectedServer) {
                        this->selectedServer = i;
                    }

                    // connect right away on double click
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        this->connect(entry);
                    }
                }

                // ping column
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("420 ms");

                ImGui::PopID();
                i++;
            }
        }

        ImGui::EndTable();
    }
}



/**
 * Draws the account management modal
 */
void ServerSelector::drawManageAccountModal(GameUI *gui) {
    bool save = false;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::SetNextWindowSize(ImVec2(555, 350));

    if(!ImGui::BeginPopupModal("Manage Account", nullptr, ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        return;
    }

    // Descriptive text
    ImGui::TextWrapped("You can change various settings of your online account here. These "
            "settings will only apply to multiplayer games.");

    ImGui::Dummy(ImVec2(0, 2));

    // Display name
    ImGui::InputText("Display Name", this->displayNameBuf.data(), this->displayNameBuf.size());
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This is the name other players will see when you connect to a server.");
    }

    // buttons
    const float spaceV = ImGui::GetContentRegionAvail().y;
    ImGui::Dummy(ImVec2(0, spaceV - 22 - 8 - 6));

    ImGui::Separator();

    if(ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
        this->showManageAccount = false;
        this->focusLayers--;
    }
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Closes this window and discards any changes you've made.");
    }
    ImGui::SameLine();
    if(ImGui::Button("Save Changes")) {
        save = true;
        ImGui::CloseCurrentPopup();
        this->showManageAccount = false;
        this->focusLayers--;
    }

    ImGui::EndPopup();

    // save prefs if needed
    if(save) {
        // display name
        const size_t len = strnlen(this->displayNameBuf.data(), this->displayNameBuf.size());
        const std::string displayName(this->displayNameBuf.data(), len);

        io::PrefsManager::setString("auth.displayName", displayName);
    }
}



/**
 * Draws a modal indicating that we need to generate a keypair, and register it with the web
 * service.
 *
 * Note that this modal _can_ be canceled, but that will also close the server selector since a
 * registered keypair is mandatory for network play.
 */
void ServerSelector::drawKeypairGenaratorModal(GameUI *gui) {
    bool clearKeys = true;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::SetNextWindowSize(ImVec2(555, 350));

    if(!ImGui::BeginPopupModal("Generate Keypair", nullptr, ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        return;
    }

    // descriptive text
    ImGui::TextWrapped("%s", "Servers require that each client has an unique public/private key pair, which is used to ensure nobody can impersonate you. "
            "The public key is stored on a web service for servers to verify.");

    ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
    ImGui::Text("Note:");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextWrapped("Your account may randomly stop working. You can rectify this by deleting all account data.");

    ImGui::Dummy(ImVec2(0,4));

    // info fields
    auto id = io::PrefsManager::getUuid("player.id");
    if(id) {
        ImGui::Bullet();
        ImGui::TextUnformatted("Player ID:");

        ImGui::PushFont(gui->getFont(GameUI::kGameFontMonospaced));
        ImGui::SameLine();
        ImGui::TextUnformatted(uuids::to_string(*id).c_str());
        ImGui::PopFont();
    }

    // inputs
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::InputText("Display Name", this->displayNameBuf.data(), this->displayNameBuf.size());
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This is the name other players will see when you connect to a server.");
    }
    ImGui::Dummy(ImVec2(0, 2));

    // buttons
    const float spaceV = ImGui::GetContentRegionAvail().y;
    ImGui::Dummy(ImVec2(0, spaceV - 22 - 8 - 6));

    ImGui::Separator();

    if(this->showLoader) {
        ImGui::TextUnformatted("Registering key...");

        ImGui::SameLine();
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2(w - 24 - 8, 0));

        ImGui::SameLine();
        ImGui::Spinner("##spin", 11, 3, ImGui::GetColorU32(ImGuiCol_Button));
    } else {
        if(ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
            this->closeRegisterModal = 0;
            this->visible = false;
        } if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Abort the keypair generation process; you will not be able to connect to servers until this is completed.");
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Generate Keys")) {
        // error if display name is empty
        if(strnlen(this->displayNameBuf.data(), this->displayNameBuf.size()) == 0) {
            this->closeRegisterModal = 2;
            clearKeys = false;
            this->registerErrorDetail = "You must enter a display name.";
            goto yeet;
        }

        // save display name
        const size_t len = strnlen(this->displayNameBuf.data(), this->displayNameBuf.size());
        const std::string displayName(this->displayNameBuf.data(), len);

        io::PrefsManager::setString("auth.displayName", displayName);

        // generate the keys, then enqueue submission to web service
        AuthManager::generateAuthKeys(false);
        this->work.enqueue(PlainRequest::RegisterKey);

        // show the network progress UI
        this->showLoader = true;
    }

yeet:;
    // success dialog
   ImGui::SetNextWindowSizeConstraints(ImVec2(420, 0), ImVec2(420, 300));
    if(this->closeRegisterModal == 1) {
        ImGui::OpenPopup("Success");
        this->showLoader = false;
        this->needsKeypairGen = false;
        this->closeRegisterModal = 0;
    }
    if(ImGui::BeginPopupModal("Success", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
        ImGui::TextWrapped("%s", "Keypair registered");
        ImGui::PopFont();

        ImGui::TextWrapped("%s", "The keypair was successfully registered. You may now connect to multi player servers.");

        ImGui::Separator();
        if(ImGui::Button("Dismiss")) {
            ImGui::CloseCurrentPopup();
            this->closeRegisterModal = 3;
        }

        ImGui::EndPopup();
    }

    // error dialog
   ImGui::SetNextWindowSizeConstraints(ImVec2(420, 0), ImVec2(420, 300));
    if(this->closeRegisterModal == 2) {
        ImGui::OpenPopup("Registration Error");
        this->showLoader = false;
        this->closeRegisterModal = 0;
        if(clearKeys) {
            AuthManager::clearAuthKeys(false);
        }
    }
    if(ImGui::BeginPopupModal("Registration Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
        ImGui::TextWrapped("%s", "Failed to register keypair");
        ImGui::PopFont();

        ImGui::TextWrapped("%s", "Something went wrong while registering the key pair. The web service may also be unavailable. Please try again later.");

        if(this->registerErrorDetail.has_value()) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
            if(ImGui::CollapsingHeader("Details")) {
                ImGui::TextWrapped("%s", this->registerErrorDetail->c_str());
                ImGui::Dummy(ImVec2(0,4));
            }
        }

        ImGui::Separator();
        if(ImGui::Button("Dismiss")) {
            ImGui::CloseCurrentPopup();
            this->closeRegisterModal = 0;
            this->registerErrorDetail = std::nullopt;
        }

        ImGui::EndPopup();
    }

    // handle closing the keygen dialog
    if(this->closeRegisterModal == 3) {
        ImGui::CloseCurrentPopup();
        this->focusLayers--;
    }

    // end
    ImGui::EndPopup();
}


/**
 * Draws the add server modal. This is basically just a text entry box for the server's IP or
 * DNS name.
 */
void ServerSelector::drawAddServerModal(GameUI *gui) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::SetNextWindowSizeConstraints(ImVec2(474, 0), ImVec2(474, 525));

    if(!ImGui::BeginPopupModal("Add Server", nullptr, ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        return;
    }

    // description text
    ImGui::TextWrapped("%s", "Enter the DNS name or IP address of a multi player server to connect "
            "to. The server will also be added to the your server list.\n"
            "If the server runs on a port other than the default, specify it by appending :1234 "
            "to the address.");

    // Path
    ImGui::Dummy(ImVec2(0,2));
    ImGui::InputText("Address", this->addServerUrl.data(), this->addServerUrl.size());
    ImGui::Dummy(ImVec2(0,2));

    // buttons
    ImGui::Separator();
    if(ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
        this->focusLayers--;
        this->showAddServer = false;
    }

    ImGui::SameLine();
    if(strnlen(this->addServerUrl.data(), this->addServerUrl.size()) &&
            ImGui::Button("Add Server")) {
        ImGui::CloseCurrentPopup();
        this->focusLayers--;
        this->showAddServer = false;

        // add the server (but don't connect)
        const size_t len = strnlen(this->addServerUrl.data(), this->addServerUrl.size());
        Server s(std::string(this->addServerUrl.data(), len));
        this->recents.servers.push_back(s);
        this->saveRecents();
        this->refreshServerStatus();
    }

    // end
    ImGui::EndPopup();

    // clear state out
    if(!this->showAddServer) {
        std::fill(this->addServerUrl.begin(), this->addServerUrl.end(), '\0');
    }
}



/**
 * Request the worker to refresh the status/ping of all servers.
 */
void ServerSelector::refreshServerStatus() {

}

/**
 * Sends a connection request to the worker thread for the given server.
 */
void ServerSelector::connect(Server &srv) {
    ConnectionReq req(srv.address);
    this->work.enqueue(req);

    this->connHost = srv.address;
    this->connStage = ConnectionStage::Dialing;
    this->isConnecting = true;
    this->focusLayers++;

    ImGui::OpenPopup("Connecting");

    // update the recents list
    srv.lastConnected = std::chrono::system_clock::now();
    srv.haveConnected = true;

    this->saveRecents();
}

/**
 * Draws the connecting modal.
 */
void ServerSelector::drawConnectingModal(GameUI *gui) {
    bool close = false;

    // constrain to center
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::SetNextWindowSize(ImVec2(640, 480));

    if(!ImGui::BeginPopupModal("Connecting", nullptr, ImGuiWindowFlags_NoCollapse | 
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        return;
    }

    // connection stage
    ImGui::PushFont(gui->getFont(GameUI::kGameFontHeading3));
    switch(this->connStage) {
        case ConnectionStage::Dialing:
            ImGui::TextUnformatted("Dialing server...");
            break;
        case ConnectionStage::Authenticating:
            ImGui::TextUnformatted("Authenticating...");
            break;
        case ConnectionStage::LoadingChunks:
            ImGui::TextUnformatted("Loading Chunks...");
            break;
        case ConnectionStage::Connected:
            ImGui::TextUnformatted("Connected!");
            break;
        case ConnectionStage::Error:
            ImGui::TextUnformatted("Connection Failed");
            break;

        default:
            ImGui::Text("Unknown %d", this->connStage);
            break;
    }
    ImGui::PopFont();

    // TODO: pretty pictures

    // progress
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::ProgressBar(this->connProgress, ImVec2(-FLT_MIN, 0), "");
    ImGui::Dummy(ImVec2(0, 2));

    // abort button
    const float spaceV = ImGui::GetContentRegionAvail().y;
    ImGui::Dummy(ImVec2(0, spaceV - 22 - 8 - 6));

    ImGui::Separator();
    if(ImGui::Button("Abort")) {
        close = true;
    }

    // show error
    if(this->connStage == ConnectionStage::Error) {
        ImGui::OpenPopup("Connection Error");

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
        ImGui::SetNextWindowSizeConstraints(ImVec2(420, 0), ImVec2(420, 350));
        if(ImGui::BeginPopupModal("Connection Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
            ImGui::PushFont(gui->getFont(GameUI::kGameFontBold));
            ImGui::TextWrapped("An error occurred while connecting to the server.");
            ImGui::PopFont();

            ImGui::TextWrapped("Check that the server address and port are correct, and that your Internet connection is working properly, then try again.");

            ImGui::Bullet();
            ImGui::TextWrapped("Server: %s", this->connHost.c_str());

            if(this->connError.has_value()) {
                ImGui::Dummy(ImVec2(0,2));

                ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
                if(ImGui::CollapsingHeader("Details")) {
                    ImGui::TextWrapped("%s", this->connError->c_str());
                    ImGui::Dummy(ImVec2(0,4));
                }
            }

            // dismiss button
            ImGui::Separator();
            if(ImGui::Button("Dismiss")) {
                ImGui::CloseCurrentPopup();
                close = true;
            }

            ImGui::EndPopup();
        }
    }

    // close the popup if needed
    if(close) {
        ImGui::CloseCurrentPopup();
        this->focusLayers--;
        this->isConnecting = false;
    }

    ImGui::EndPopup();
}

/**
 * Server selector worker thread; this is mainly used to handle the network IO so we don't block
 * the UI layer.
 */
void ServerSelector::workerMain() {
    // set up thread
    util::Thread::setName("Server Picker Worker");
    MUtils::Profiler::NameThread("Server Picker Worker");

    // get requests
    while(this->workerRun) {
        WorkItem i;
        this->work.wait_dequeue(i);

        // plain requests
        if(std::holds_alternative<PlainRequest>(i)) {
            switch(std::get<PlainRequest>(i)) {
                case PlainRequest::RegisterKey:
                    this->workerRegisterKey();
                    break;

                default:
                    XASSERT(false, "Unhandled request");
                    break;
            }
        }
        // connecting to a server
        else if(std::holds_alternative<ConnectionReq>(i)) {
            const auto req = std::get<ConnectionReq>(i);

            try {
                auto server = std::make_shared<net::ServerConnection>(req.host);

                // authenticate
                this->connStage = ConnectionStage::Authenticating;
                auto success = server->authenticate();
                XASSERT(success, "Failed to authenticate (should not get here...)");

                // create the remote world source
                const auto numWorkers = io::PrefsManager::getUnsigned("world.sourceWorkThreads", 2);
                const auto playerId = web::AuthManager::getPlayerId();
                auto source = std::make_shared<world::RemoteSource>(server, playerId, numWorkers);
                this->connectedWorld = source;

                // load the basic chunks around us
                this->connStage = ConnectionStage::LoadingChunks;
                this->connProgress = 0;

                // done! open the world
                this->connStage = ConnectionStage::Connected;

                // allow pending packets to yeet
                std::this_thread::sleep_for(std::chrono::milliseconds(333));
                this->wantsOpenWorld = true;
            } catch(std::exception &e) {
                Logging::error("Failed to connect to server {}: {}", req.host, e.what());

                this->connError = e.what();
                this->connStage = ConnectionStage::Error;
            }
        }
    }

    // clean up
    MUtils::Profiler::FinishThread();
}

/**
 * Registers the player ID and public key with the web service.
 */
void ServerSelector::workerRegisterKey() {
    try {
        // make network request to register keys
        AuthManager::registerAuthKeys(true);

        // show success dialog
        this->closeRegisterModal = 1;
    } catch(std::exception &e) {
        Logging::error("Failed to register keys: {}", e.what());

        // show error dialog
        this->registerErrorDetail = e.what();
        this->closeRegisterModal = 2;
    }

}
