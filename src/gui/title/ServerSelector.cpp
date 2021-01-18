#include "ServerSelector.h"
#include "TitleScreen.h"
#include "gui/GameUI.h"
#include "gui/Loaders.h"

#include "web/AuthManager.h"
#include "util/Thread.h"

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

    // check if keypair must be generated
    if(!AuthManager::areKeysAvailable()) {
        this->needsKeypairGen = true;
        this->focusLayers++;
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
        // TODO: open the server connection dialog
    } if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enter the address of a new server to connect to");
    }

    // various modales
    if(this->needsKeypairGen) {
        ImGui::OpenPopup("Generate Keypair");
        this->drawKeypairGenaratorModal(gui);
    }

    // clean up
    ImGui::End();
}

/**
 * Draws the account actions toolbar, as well as the progress indicator, at the top of the window.
 */
void ServerSelector::drawAccountBar(GameUI *gui) {
    if(ImGui::Button("Manage Account")) {
        // TODO: show account details
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
        ImGuiTableFlags_BordersOuter | (!this->recents.empty() ? ImGuiTableFlags_Sortable : 0);
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
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 42);

            ImGui::TableHeadersRow();
            ImGui::TableSetupScrollFreeze(0, 1);

            // for each server, list it
            size_t i = 0;
            for(const auto &entry : this->recents.servers) {
                // begin a new row
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i++);

                std::time_t opened = std::chrono::system_clock::to_time_t(entry.lastConnected);
                const auto openedTm = localtime(&opened);

                char timeBuf[64];
                memset(&timeBuf, 0, sizeof(timeBuf));
                strftime(timeBuf, 63, "%c", openedTm);

                const auto str = f("{}\nLast Connected: {}", entry.address, timeBuf);

                if(ImGui::Selectable(str.c_str(), (this->selectedServer == i), ImGuiSelectableFlags_AllowDoubleClick)) {
                    if(i != this->selectedServer) {
                        this->selectedServer = i;
                    }

                    // connect right away on double click
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        // TODO: connect
                    }
                }

                // ping column
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Ping");

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
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
        // generate the keys, then enqueue submission to web service
        AuthManager::generateAuthKeys(false);
        this->work.enqueue(PlainRequest::RegisterKey);

        // show the network progress UI
        this->showLoader = true;
    }

    // success dialog
   ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(400, 300));
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
   ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(400, 300));
    if(this->closeRegisterModal == 2) {
        ImGui::OpenPopup("Registration Error");
        this->showLoader = false;
        this->closeRegisterModal = 0;
        AuthManager::clearAuthKeys(false);
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
