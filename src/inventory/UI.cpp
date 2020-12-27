#include "UI.h"
#include "UIBar.h"
#include "UIDetail.h"
#include "Manager.h"

#include "gui/GameUI.h"
#include "world/block/BlockRegistry.h"
#include "gfx/gl/texture/Texture2D.h"

#include <imgui.h>

#include <vector>
#include <cstddef>

using namespace inventory;

/**
 * Initializes the subcomponents of the inventory UI.
 */
UI::UI(Manager *_mgr) : inventory(_mgr) {
    _mgr->ui = this;

    // set up UI components
    this->bar = new UIBar(this);
    this->detail = new UIDetail(this);

    // create the atlas texture
    this->atlas = new gfx::Texture2D;
    this->atlas->setUsesLinearFiltering(false);
    this->atlas->setDebugName("InventoryAtlas");

    this->uploadAtlasTexture();
}

/**
 * Removes inventory observers
 */
UI::~UI() {
    // delete our subcomponents
    delete this->bar;
    delete this->detail;

    delete this->atlas;
}

/**
 * Regenerates the atlas texture.
 */
void UI::uploadAtlasTexture() {
    glm::ivec2 atlasSize;
    std::vector<std::byte> data;

    // generate the data and upload it
    world::BlockRegistry::generateInventoryTextureAtlas(atlasSize, data);

    this->atlas->allocateBlank(atlasSize.x, atlasSize.y, gfx::Texture2D::RGBA16F);
    this->atlas->bufferSubData(atlasSize.x, atlasSize.y, 0, 0,  gfx::Texture2D::RGBA16F, data.data());

    // generate mipmaps
    this->atlas->bind();
    gl::glGenerateMipmap(gl::GL_TEXTURE_2D);
    gl::glTexParameterf(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.f);
    gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR_MIPMAP_LINEAR);
}



/**
 * Draws the inventory UI.
 *
 * If the editor is open, we start a modal session first, then draw the window and bar. Otherwise,
 * only the bar is drawn.
 */
void UI::draw(gui::GameUI *ui) {
    // draw the bar at the bottom
    bool showDetail = (this->showsDetail | this->shouldClose);
    bool toClose = this->bar->draw(ui, !showsDetail);

    // begin a modal session above which to draw all the things
    if(showDetail) {
        glm::vec2 center(ImGui::GetIO().DisplaySize.x * .5, ImGui::GetIO().DisplaySize.y * .5);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, glm::vec2(.5, .5));

        ImGui::OpenPopup("Inventory");
        if(ImGui::BeginPopupModal("Inventory", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
            this->detail->draw(ui);

            if(this->shouldClose) {
                ImGui::CloseCurrentPopup();
                this->shouldClose = false;
            }

            ImGui::EndPopup();
        }
    }

    // close out window context
    if(toClose) {
        ImGui::End();
    }
}

