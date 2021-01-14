#include "WorldRenderer.h"
#include "WorldRendererDebugger.h"
#include "RenderStep.h"
#include "scene/SceneRenderer.h"

#include "input/InputManager.h"
#include "input/BlockInteractions.h"
#include "input/PlayerPosPersistence.h"
#include "gui/GameUI.h"
#include "gui/MenuBarHandler.h"
#include "gui/MainWindow.h"
#include "gui/Loaders.h"
#include "gui/title/TitleScreen.h"

#include "render/chunk/VertexGenerator.h"
#include "world/FileWorldReader.h"
#include "world/WorldSource.h"
#include "world/TimePersistence.h"
#include "world/generators/Terrain.h"
#include "physics/Engine.h"
#include "physics/EngineDebugRenderer.h"
#include "inventory/Manager.h"
#include "inventory/UI.h"

#include "steps/FXAA.h"
#include "steps/Lighting.h"
#include "steps/HDR.h"
#include "steps/SSAO.h"
#include "particles/Renderer.h"

#include "gfx/gl/buffer/FrameBuffer.h"
#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <glm/ext.hpp>
#include <SDL.h>
#include <imgui.h>

using namespace render;

std::shared_ptr<SceneRenderer> gSceneRenderer = nullptr;
std::shared_ptr<Lighting> gLightRenderer = nullptr;
std::shared_ptr<particles::Renderer> gParticleRenderer = nullptr;
inventory::Manager *gInventoryManager = nullptr;

/**
 * Creates the renderer resources.
 */
WorldRenderer::WorldRenderer(gui::MainWindow *_win, std::shared_ptr<gui::GameUI> &_gui,
        std::shared_ptr<world::WorldSource> &_source) : window(_win), gui(_gui), source(_source) {
    // set up the vertex generator; it needs to create a GL context
    render::chunk::VertexGenerator::init(_win);

    // create the IO manager
    this->input = new input::InputManager(_win);

    // then, the render steps
    auto scnRnd = std::make_shared<SceneRenderer>();
    gSceneRenderer = scnRnd;

    scnRnd->setWorldSource(this->source);
    this->steps.push_back(scnRnd);

    auto ssao = std::make_shared<SSAO>();
    this->steps.push_back(ssao);

    auto physDbg = std::make_shared<physics::EngineDebugRenderer>();
    this->steps.push_back(physDbg);

    this->lighting = std::make_shared<Lighting>();
    gLightRenderer = this->lighting;
    this->steps.push_back(this->lighting);

    auto particles = std::make_shared<particles::Renderer>();
    gParticleRenderer = particles;
    this->steps.push_back(particles);

    this->hdr = std::make_shared<HDR>();
    this->steps.push_back(this->hdr);

    this->fxaa = std::make_shared<FXAA>();
    this->steps.push_back(this->fxaa);

    // Set up some shared buffers
    this->lighting->setSceneRenderer(scnRnd);
    this->lighting->setOcclusionTex(ssao->occlusionTex);

    this->hdr->setDepthBuffer(this->lighting->gDepth);
    this->hdr->setOutputFBO(this->fxaa->getFXAABuffer());

    ssao->setDepthTex(this->lighting->gDepth);
    ssao->setNormalTex(this->lighting->gNormal);

    this->debugger = new WorldRendererDebugger(this);

    // load the current world time
    this->timeSaver = new world::TimePersistence(this->source, &this->time);

    // interactions and some game UI
    this->physics = new physics::Engine(scnRnd, &this->camera);
    this->physics->setDebugRenderStep(physDbg);
    scnRnd->setPhysicsEngine(this->physics);
    particles->setPhysicsEngine(this->physics);

    this->inventory = new inventory::Manager(this->input);
    this->inventory->loadInventory(this->source);
    gInventoryManager = this->inventory;

    this->inventoryUi = std::make_shared<inventory::UI>(this->inventory);
    _gui->addWindow(this->inventoryUi);

    this->blockInt = new input::BlockInteractions(scnRnd, this->source, this->inventory);

    glm::vec3 loadedPos, loadedAngles;
    this->posSaver = new input::PlayerPosPersistence(this->input, source);
    if(this->posSaver->loadPosition(loadedPos)) {
        this->physics->setPlayerPosition(loadedPos);
    } else {
        this->physics->setPlayerPosition(glm::vec3(-10, 80, -10));
    }

    this->debugItemToken = gui::MenuBarHandler::registerItem("World", "World Renderer Debug", &this->isDebuggerOpen);
}
/**
 * Releases all of our render resources.
 */
WorldRenderer::~WorldRenderer() {
    if(this->pauseWin) {
        this->gui->removeWindow(this->pauseWin);
    }

    delete this->timeSaver;

    this->source->flushDirtyChunksSync();

    if(this->debugItemToken) {
        gui::MenuBarHandler::unregisterItem(this->debugItemToken);
    }
    if(this->debugger) {
        delete this->debugger;
    }

    delete this->posSaver;

    delete this->blockInt;

    this->gui->removeWindow(this->inventoryUi);
    this->inventoryUi = nullptr;

    gInventoryManager = nullptr;
    delete this->inventory;

    delete this->physics;

    this->lighting = nullptr;
    gLightRenderer = nullptr;
    this->hdr = nullptr;
    this->fxaa = nullptr;

    gParticleRenderer = nullptr;
    gSceneRenderer = nullptr;
    this->steps.clear();

    render::chunk::VertexGenerator::shutdown();

    delete this->input;
    this->source = nullptr;
}

/**
 * Prepare the world for rendering.
 */
void WorldRenderer::willBeginFrame() {
    // update the inputs and camera as well as camera display angles
    this->input->startFrame();
    this->camera.startFrame();

    // pass position deltas to the physics engine for the player physics; then update view
    const glm::vec3 angles = this->input->getEulerAngles();
    const glm::vec3 deltas = this->input->getMovementDelta();

    this->camera.updateAngles(angles, this->input->getNonpitchEulerAngles());

    this->physics->movePlayer(deltas, this->input->shouldJump());
    this->physics->startFrame();

    this->updateView();

    // start of frame for render steps
    this->source->startOfFrame();
    this->posSaver->startOfFrame(this->camera.getCameraPosition());

    for(auto &step : this->steps) {
        step->startOfFrame();
    }

    // draw debuggre ig
    if(this->debugger && this->isDebuggerOpen) {
        this->debugger->draw();
    }

    // increment time (XXX: make this more accurate)
    if(!this->paused) {
        // @60fps: 60 sec * 24 min
        this->time += 1./(3600. * 24);
    }

    // pause menu stuff
    this->animatePauseMenu();
    if(this->exitToTitle) {
        this->exitToTitle++;
    }

    if(this->isPauseMenuOpen && this->exitToTitle == 9) {
        // force writing out inventory, dirty chunks
        if(this->inventory->isDirty()) {
            this->inventory->writeInventory();
        }
    }
    else if(this->isPauseMenuOpen && this->exitToTitle == 10) {
        // switch to title screen
        auto title = std::make_shared<gui::TitleScreen>(this->window, this->gui);
        this->window->setPrimaryStep(title);

        this->exitToTitle = 0;
    }

    // perform transfers of chunk datas
    render::chunk::VertexGenerator::startOfFrame();
}

/**
 * Handle the drawing stages.
 */
void WorldRenderer::draw() {
    // perform the pre-render, render and post-render stages
    for(auto step : this->steps) {
        // unbind any existing framebuffers
        gfx::FrameBuffer::unbindRW();

        // do we need to bind the G-buffer?
        if(step->requiresBoundGBuffer()) {
            this->lighting->bindGBuffer();
        }
        // do we need to bind the HDR buffer?
        else if(step->requiresBoundHDRBuffer()) {
            this->hdr->bindHDRBuffer();
        }

        // execute pre-render steps
        step->preRender(this);

        // render
        step->render(this);

        // clean up
        step->postRender(this);

        if(step->requiresBoundGBuffer()) {
            this->lighting->unbindGBuffer();
        } else if(step->requiresBoundHDRBuffer()) {
            this->hdr->unbindHDRBuffer();
        }
    }
}

/**
 * Resize all of our buffers as needed.
 */
void WorldRenderer::reshape(unsigned int width, unsigned int height) {
    Logging::trace("Resizing world render buffers: {:d}x{:d}", width, height);

    this->viewportWidth = width;
    this->viewportHeight = height;
    this->aspect = ((double) width) / ((double) height);

    // reshape all render steps as well
    for(auto &step : this->steps) {
        step->viewportSize = glm::vec2(width, height);
        step->reshape(width, height);
    }
}

/**
 * Do nothing with UI events.
 */
bool WorldRenderer::handleEvent(const SDL_Event &event) {
    // game inputs
    if(this->input->acceptsGameInput()) {
        if(this->blockInt->handleEvent(event)) {
            return true;
        }
    }

    // handle a few special key events
    if(event.type == SDL_KEYDOWN) {
        const auto &k = event.key.keysym;

        // ESC toggles the pause menu
        if(k.scancode == SDL_SCANCODE_ESCAPE) {
            // close pause menu if it's open
            if(this->isPauseMenuOpen) {
                this->closePauseMenu();
            }
            // open if no other cursor-requiring stuff is open
            else {
                if(!this->input->getCursorCount()) {
                    this->openPauseMenu();
                }
            }
        }
        // F3 toggles the scene debugging overlays
        else if(k.scancode == SDL_SCANCODE_F3) {
            gSceneRenderer->toggleDebugOverlays();
        }
        // F9 toggles menu bar
        else if(k.scancode == SDL_SCANCODE_F9) {
            bool vis = gui::MenuBarHandler::isVisible();

            if(vis) {
                this->input->decrementCursorCount();
            } else {
                this->input->incrementCursorCount();
            }
            vis = !vis;

            gui::MenuBarHandler::setVisible(vis);
        }
    }

    // various UIs
    if(this->inventory->handleEvent(event)) {
        return true;
    }

    // send it to the IO handler
    return this->input->handleEvent(event);
}



/**
 * Updates the world view matrices.
 */
void WorldRenderer::updateView() {
    PROFILE_SCOPE(UpdateView);

    // finally, update camera view matrix and the projection matrix
    this->camera.updateViewMatrix();

    float width = (float) this->viewportWidth;
    float height = (float) this->viewportHeight;

    this->projection = glm::perspective(glm::radians(this->projFoV), (float) (width / height), 
            this->zNear, this->zFar);


    // give each of the stages some information from the camera for rendering
    for(auto stage : this->steps) {
        stage->viewMatrix = this->camera.getViewMatrix();
        stage->viewPosition = this->camera.getShiftedCameraPosition();
        stage->viewLookAt = this->camera.getCameraLookAt();
        stage->viewDirection = this->camera.getCameraFront();
        stage->viewUp = this->camera.getCameraUp();

        stage->projectionMatrix = this->projection;
    }
}



/**
 * Draws the pause menu buttons.
 */
void WorldRenderer::drawPauseButtons(gui::GameUI *gui) {
    // begin the window
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    // ImGui::SetNextWindowSize(ImVec2(402, 0));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.);

    if(!ImGui::Begin("Pause Menu Buttons", nullptr, windowFlags)) {
        return;
    }

    const ImVec2 btnSize(400, 0);
    const auto btnFont = gui->getFont(gui::GameUI::kGameFontHeading2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.1, 0, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.25, 0, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.33, .066, .066, .9));

    // close the menu
    ImGui::PushFont(btnFont);
    if(ImGui::Button("Return to Game", btnSize)) {
        this->closePauseMenu();
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Closes this menu so you can get back to playing with cubes");
    }

    // return to title
    ImGui::Dummy(ImVec2(0,10));
    ImGui::PushFont(btnFont);
    if(ImGui::Button("Exit to Main Menu", btnSize)) {
        // set a flag to perform this change next frame
        this->exitToTitle = 1;
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Saves the current world and returns to the title screen");
    }

    ImGui::PopStyleColor(3);

    // draw the "closing shit" message
    if(this->exitToTitle > 0) {
        ImGui::OpenPopup("Exiting");

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));
        ImGui::SetNextWindowSizeConstraints(ImVec2(325, 0), ImVec2(325, 500));

        if(ImGui::BeginPopupModal("Exiting", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushFont(gui->getFont(gui::GameUI::kGameFontBold));
            ImGui::Spinner("##spin", 9, 3, ImGui::GetColorU32(ImGuiCol_Button));
            ImGui::SameLine();
            ImGui::TextUnformatted("Please wait...");
            ImGui::PopFont();

            ImGui::TextWrapped("%s", "Waiting for background work to complete. This may take a few seconds.");

            ImGui::EndPopup();
        }
    }

    // done
    ImGui::End();

    ImGui::PopStyleVar();
}

/**
 * Animates the pause menu.
 */
void WorldRenderer::animatePauseMenu() {
    using namespace std::chrono;

    // bail if pause menu is not animating
    if(!this->isPauseMenuAnimating) return;

    const auto now = steady_clock::now();
    const auto diff = duration_cast<milliseconds>(now - this->menuOpenedAt).count();
    const float diffSecs = ((float) diff) / 1000.;

    // stop animating if we've been a whole animation period
    if(diffSecs >= kPauseAnimationDuration) {
        this->isPauseMenuAnimating = false;

        this->hdr->setVignetteParams(.33, .5);
        this->hdr->setHsvAdjust(glm::vec3(0, .26, .75));
    }
    // otherwise, fade out the saturation
    else {
        const float frac = std::min((diffSecs / kPauseAnimationDuration) + .1, 1.);
        const auto sqt = frac*frac;
        const auto t = sqt / (2. * (sqt - frac) + 1.f);

        this->hdr->setVignetteParams(1 - (.67 * t), std::min(.5 ,(t * 2.5) * .5));
        this->hdr->setHsvAdjust(glm::vec3(0, 1. - (.74 * t), 1 - (.25 * t)));
    }
}

/**
 * Opens the pause menu.
 */
void WorldRenderer::openPauseMenu() {
    // open pause menu
    this->isPauseMenuOpen = true;
    this->paused = true;

    if(!this->pauseWin) {
        this->pauseWin = std::make_shared<PauseWindow>(this);
        this->gui->addWindow(this->pauseWin);
    }

    this->pauseWin->setVisible(true);

    // start the timer for animating
    this->isPauseMenuAnimating = true;
    this->menuOpenedAt = std::chrono::steady_clock::now();

    this->input->incrementCursorCount();
}

/**
 * Closes the pause menu.
 */
void WorldRenderer::closePauseMenu() {
    // close pause menu
    this->isPauseMenuOpen = false;
    this->isPauseMenuAnimating = false;
    this->paused = false;

    this->pauseWin->setVisible(false);

    // restore the saturation of the game content
    this->hdr->setHsvAdjust(glm::vec3(0,1,1));
    this->hdr->setVignetteParams(1, 0);

    this->input->decrementCursorCount();
}
