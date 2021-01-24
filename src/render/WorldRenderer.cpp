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
#include "gui/InGamePrefsWindow.h"
#include "gui/DisconnectedError.h"
#include "gui/title/TitleScreen.h"
#include "chat/Manager.h"
#include "render/chunk/VertexGenerator.h"
#include "world/FileWorldReader.h"
#include "world/ClientWorldSource.h"
#include "world/TimePersistence.h"
#include "world/generators/Terrain.h"
#include "physics/Engine.h"
#include "physics/EngineDebugRenderer.h"
#include "inventory/Manager.h"
#include "inventory/UI.h"
#include "io/PathHelper.h"
#include "io/PrefsManager.h"
#include "util/Easing.h"

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

#include <cstdio>
#include <filesystem>

#include <jpeglib.h>

using namespace render;

std::shared_ptr<SceneRenderer> gSceneRenderer = nullptr;
std::shared_ptr<Lighting> gLightRenderer = nullptr;
std::shared_ptr<particles::Renderer> gParticleRenderer = nullptr;
inventory::Manager *gInventoryManager = nullptr;

/**
 * Creates the renderer resources.
 */
WorldRenderer::WorldRenderer(gui::MainWindow *_win, std::shared_ptr<gui::GameUI> &_gui,
        std::shared_ptr<world::ClientWorldSource> &_source) : window(_win), gui(_gui), source(_source) {
    std::shared_ptr<SSAO> ssao = nullptr;
    const bool wantSsao = io::PrefsManager::getBool("gfx.ssao", true);

    // set up the vertex generator; it needs to create a GL context
    render::chunk::VertexGenerator::init(_win);

    // create the IO manager
    this->input = new input::InputManager(_win);

    // then, the render steps
    auto scnRnd = std::make_shared<SceneRenderer>();
    gSceneRenderer = scnRnd;

    scnRnd->setWorldSource(this->source);
    this->steps.push_back(scnRnd);

    if(wantSsao) {
        ssao = std::make_shared<SSAO>();
        this->steps.push_back(ssao);
    }

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

    this->hdr->setDepthBuffer(this->lighting->gDepth);
    this->hdr->setOutputFBO(this->fxaa->getFXAABuffer());

    if(ssao) {
        this->lighting->setOcclusionTex(ssao->occlusionTex);
        ssao->setDepthTex(this->lighting->gDepth);
        ssao->setNormalTex(this->lighting->gNormal);
    }

    this->debugger = new WorldRendererDebugger(this);

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

    // get the world load position and spawn positions
    auto spawnProm = this->source->getSpawnPosition();
    const auto spawn = spawnProm.get_future().get();

    // restore 1P state
    if(this->source->isSinglePlayer()) {
        this->timeSaver = new world::TimePersistence(this->source, &this->source->currentTime);

        glm::vec3 loadedPos, loadedAngles;
        this->posSaver = new input::PlayerPosPersistence(this->input, source);
        if(this->posSaver->loadPosition(loadedPos)) {
            this->physics->setPlayerPosition(loadedPos);
        } else {
            this->physics->setPlayerPosition(spawn.first);
        }
    }
    // for multiplayer worlds, ask the server for our starting position
    else {
        auto prom = this->source->getInitialPosition();
        auto pair = prom.get_future().get();

        this->physics->setPlayerPosition(pair.first);
        this->input->setAngles(pair.second);
    }

    // set up 2P specific UI
    if(!this->source->isSinglePlayer()) {
        this->chat = new chat::Manager(this->input, this->gui, this->source);
    }

    this->debugItemToken = gui::MenuBarHandler::registerItem("World", "World Renderer Debug", &this->isDebuggerOpen);

    // load preferences and work queue
    this->workerRun = true;
    this->worker = std::make_unique<std::thread>(std::bind(&WorldRenderer::workerMain, this));

    this->loadPrefs();
}
/**
 * Releases all of our render resources.
 */
WorldRenderer::~WorldRenderer() {
    this->workerRun = false;
    this->work.enqueue(WorkItem());
    this->worker->join();
    this->worker = nullptr;

    if(this->pauseWin) {
        this->gui->removeWindow(this->pauseWin);
    }

    if(this->posSaver) delete this->posSaver;
    if(this->timeSaver) delete this->timeSaver;

    this->source->flushDirtyChunksSync();
    this->source->shutDown();

    if(this->chat) delete chat;

    if(this->debugItemToken) {
        gui::MenuBarHandler::unregisterItem(this->debugItemToken);
    }
    if(this->debugger) {
        delete this->debugger;
    }

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

    render::chunk::VertexGenerator::shutdown();

    delete this->input;
    this->source = nullptr;

    this->steps.clear();

    // release the pause screenshot data
    if(this->screenshot) {
        delete[] this->screenshot;
        this->screenshot = nullptr;
    }
}

/**
 * Loads world renderer preferences.
 */
void WorldRenderer::loadPrefs() {
    using namespace io;

    this->projFoV = PrefsManager::getFloat("gfx.fov", 74.);

    // calculate the zFar for render distance
    const auto renderDist = PrefsManager::getUnsigned("world.render.distance", 2);
    this->zFar = 400. * renderDist;

    this->lighting->setFogOffset(205. * renderDist);

    if(gSceneRenderer) {
        gSceneRenderer->loadPrefs();
    }
    if(this->fxaa) {
        this->fxaa->loadPrefs();
    }

    this->inventoryUi->loadPrefs();
    this->window->loadPrefs();
}

/**
 * Prepare the world for rendering.
 */
void WorldRenderer::willBeginFrame() {
    using namespace std::chrono;

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
    if(!this->source->isValid()) {
        auto title = std::make_shared<gui::TitleScreen>(this->window, this->gui);
        this->window->setPrimaryStep(title);

        const auto desc = this->source->getErrorStr();

        Logging::error("World source became invalid: {}", desc);

        // pop up an error
        auto err = std::make_shared<gui::DisconnectedError>(desc);
        err->setSelf(err);
        this->gui->addWindow(err);
        return;
    }

    this->source->playerMoved(this->camera.getCameraPosition(), this->input->getAngles());
    if(this->posSaver) {
        this->posSaver->startOfFrame(this->camera.getCameraPosition());
    }

    for(auto &step : this->steps) {
        step->startOfFrame();
    }

    // draw debuggre ig
    if(this->debugger && this->isDebuggerOpen) {
        this->debugger->draw();
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
        if(this->needsQuit) {
            this->window->quit();
        } else {
            auto title = std::make_shared<gui::TitleScreen>(this->window, this->gui);
            this->window->setPrimaryStep(title);
        }

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

    // screenshot time!
    if(this->needsScreenshot) {
        this->captureScreenshot();
        this->needsScreenshot = false;

        // save it as well if it's a quitting time screenshot
        if(this->isQuitting) {
            this->saveScreenshot();
        }
    }
}

/**
 * When we're about to quit, force a screenshot on the next render loop iteration. (Main window
 * guarantees that we'll have at least one more frame drawn before quitting, after this method is
 * invoked by it)
 */
void WorldRenderer::willQuit() {
    this->needsScreenshot = true;
    this->isQuitting = true;
}

/**
 * Reload prefs if needed. We defer this until now to prevent some graphical artifacts.
 */
void WorldRenderer::willEndFrame() {
    if(this->needsPrefsLoad) {
        this->loadPrefs();
        this->needsPrefsLoad = false;
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
    } else if(this->chat && this->chat->handleEvent(event)) {
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
    const ImVec2 btnSize2(196, 0);
    const auto btnFont = gui->getFont(gui::GameUI::kGameFontHeading2);

    // close the menu
    ImGui::PushFont(btnFont);
    if(ImGui::Button("Return to Game", btnSize)) {
        this->closePauseMenu();
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Closes this menu so you can get back to playing with cubes");
    }

    // Preferences
    ImGui::Dummy(ImVec2(0,10));
    ImGui::PushFont(btnFont);
    if(ImGui::Button("Preferences", btnSize)) {
        if(!this->prefsWin) {
            this->prefsWin = std::make_shared<gui::InGamePrefsWindow>(this);
            this->gui->addWindow(this->prefsWin);
        }

        this->prefsWin->setVisible(true);
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Change a subset of game settings");
    }

    // return to title
    ImGui::Dummy(ImVec2(0,10));
    ImGui::PushFont(btnFont);
    if(ImGui::Button("Main Menu", btnSize2)) {
        this->exitToTitle = 1;
        this->saveScreenshot();
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Saves the current world and returns to the title screen");
    }

    ImGui::SameLine();
    ImGui::PushFont(btnFont);
    if(ImGui::Button("Quit", btnSize2)) {
        this->exitToTitle = 1;
        this->needsQuit = true;
        this->saveScreenshot();
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Saves the current world and exits the game");
    }

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
        const auto t = util::Easing::easeInQuad(frac);
        const auto t2 = util::Easing::easeInOutCubic(frac);

        this->hdr->setVignetteParams(1 - (.67 * t), std::min(.5 ,(t * 2.5) * .5));
        this->hdr->setHsvAdjust(glm::vec3(0, 1. - (.74 * t2), 1 - (.25 * t2)));
    }
}

/**
 * Opens the pause menu.
 */
void WorldRenderer::openPauseMenu() {
    // open pause menu
    this->isPauseMenuOpen = true;
    this->source->setPaused(true);

    if(!this->pauseWin) {
        this->pauseWin = std::make_shared<PauseWindow>(this);
        this->gui->addWindow(this->pauseWin);
    }

    this->pauseWin->setVisible(true);

    // start the timer for animating
    this->isPauseMenuAnimating = true;
    this->menuOpenedAt = std::chrono::steady_clock::now();

    this->input->incrementCursorCount();

    // TODO: take a screenshot, which we compress and save to disk for the world
    this->needsScreenshot = true;
}

/**
 * Closes the pause menu.
 */
void WorldRenderer::closePauseMenu() {
    // close pause menu
    this->isPauseMenuOpen = false;
    this->isPauseMenuAnimating = false;
    this->source->setPaused(false);

    this->pauseWin->setVisible(false);

    if(this->prefsWin) {
        this->prefsWin->setVisible(false);
    }

    // restore the saturation of the game content
    this->hdr->setHsvAdjust(glm::vec3(0,1,1));
    this->hdr->setVignetteParams(1, 0);

    this->input->decrementCursorCount();

    // release the pause screenshot data
    if(this->screenshot) {
        delete[] this->screenshot;
        this->screenshot = nullptr;
    }
}

/**
 * Captures a screenshot of the currently bound framebuffer. We assume this is the main window
 * framebuffer.
 */
void WorldRenderer::captureScreenshot() {
    using namespace gl;
    PROFILE_SCOPE(CaptureScreenshot);

    // bail if we've already one (if we're in menu already and we try to quit, for example)
    if(this->screenshot) return;

    // allocate the buffer
    const size_t w = this->viewportWidth, h = this->viewportHeight;

    std::byte *buf = new std::byte[w * h * 3];

    // get the pixels
    {
        PROFILE_SCOPE_STR("glReadPixels", 0xFF0000FF);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, buf);
    }

    // store it for later
    XASSERT(!this->screenshot, "Attempting to write out duplicate pause screenshots");
    this->screenshot = buf;
}

/**
 * Saves the screenshot on the worker thread. This should be called right when it's apparent that
 * we'll be going back to the main menu/exiting the level.
 */
void WorldRenderer::saveScreenshot() {
    // put together a work request
    SaveScreenshot save;

    save.data = this->screenshot;
    save.size = glm::ivec2(this->viewportWidth, this->viewportHeight);

    this->work.enqueue(save);

    // clean up
    this->screenshot = nullptr;
}



/**
 * Worker main function
 */
void WorldRenderer::workerMain() {
    util::Thread::setName("World Renderer Worker");
    MUtils::Profiler::NameThread("World Renderer Worker");

    // collect work requests
    while(this->workerRun) {
        WorkItem i;
        this->work.wait_dequeue(i);

        if(std::holds_alternative<std::monostate>(i)) {
            // nothing
        }
        // save a screenshot
        else if(std::holds_alternative<SaveScreenshot>(i)) {
            const auto &save = std::get<SaveScreenshot>(i);
            this->workerSaveScreenshot(save);
        }
    }

    // clean up
    MUtils::Profiler::FinishThread();
}

/**
 * Performs encoding of the given bitmap to JPEG 2000.
 */
void WorldRenderer::workerSaveScreenshot(const SaveScreenshot &save) {
    FILE *file = nullptr;

    size_t numRows = save.size.y;
    JSAMPROW rowPtrs[numRows];

    // prepare the input data
    auto idProm = this->source->getWorldInfo("world.id");
    auto idFuture = idProm.get_future();
    const auto waitStatus = idFuture.wait_for(std::chrono::seconds(2));

    if(waitStatus == std::future_status::timeout) {
        Logging::error("Gave up getting world id to save screenshot: {}", waitStatus);

        delete[] save.data;
        return;
    }

    const auto worldIdBytes = idFuture.get();
    const std::string worldId(worldIdBytes.begin(), worldIdBytes.end());

    std::filesystem::path path(io::PathHelper::cacheDir());
    path /= f("worldpreview-{}.jpg", worldId);

    // set up the encoder and error handler
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    // write to an stdio stream
    file = fopen(path.string().c_str(), "wb");
    if(!file) {
        Logging::error("Failed to open world screenshot '{}' for writing", path.string());
        goto beach;
    }
    jpeg_stdio_dest(&cinfo, file);

    // configure compression parameters
    cinfo.image_width = save.size.x;
    cinfo.image_height = save.size.y;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, kPreviewQuality, true);

    // begin compression for all rows
    jpeg_start_compress(&cinfo, TRUE);

    for(size_t i = 0; i < numRows; i++) {
        rowPtrs[i] = reinterpret_cast<JSAMPROW>(save.data + ((numRows - 1 - i) * (save.size.x * 3)));
    }

    jpeg_write_scanlines(&cinfo, rowPtrs, numRows);

    // finish up compression
    jpeg_finish_compress(&cinfo);

    // clean up
beach:;
    if(file) fclose(file);
    jpeg_destroy_compress(&cinfo);

    delete[] save.data;
}
