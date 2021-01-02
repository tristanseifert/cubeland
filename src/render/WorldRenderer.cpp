#include "WorldRenderer.h"
#include "WorldRendererDebugger.h"
#include "RenderStep.h"
#include "scene/SceneRenderer.h"

#include "input/InputManager.h"
#include "input/BlockInteractions.h"
#include "input/PlayerPosPersistence.h"
#include "gui/GameUI.h"

#include "world/FileWorldReader.h"
#include "world/WorldSource.h"
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

using namespace render;

std::shared_ptr<SceneRenderer> gSceneRenderer = nullptr;

/**
 * Creates the renderer resources.
 */
WorldRenderer::WorldRenderer(gui::MainWindow *win, std::shared_ptr<gui::GameUI> &_gui) :
    gui(_gui) {
    // XXX: testing world source
    try {
        auto file = std::make_shared<world::FileWorldReader>("/Users/tristan/cubeland/build/./test.world");
        auto gen = std::make_shared<world::Terrain>(420);

        this->source = std::make_shared<world::WorldSource>(file, gen);
        // this->source->setGenerateOnly(true);
    } catch(std::exception &e) {
        Logging::error("Failed to open world: {}", e.what());
        exit(-1);
    }

    // create the IO manager
    this->input = new input::InputManager(win);

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
    this->steps.push_back(this->lighting);

    auto particles = std::make_shared<particles::Renderer>();
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

#ifndef NDEBUG
    this->debugger = new WorldRendererDebugger(this);
#endif

    // interactions and some game UI
    this->physics = new physics::Engine(scnRnd, &this->camera);
    this->physics->setDebugRenderStep(physDbg);
    scnRnd->setPhysicsEngine(this->physics);
    particles->setPhysicsEngine(this->physics);

    this->inventory = new inventory::Manager(this->input);
    this->inventory->loadInventory(this->source);

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
}
/**
 * Releases all of our render resources.
 */
WorldRenderer::~WorldRenderer() {
    if(this->debugger) {
        delete this->debugger;
    }

    delete this->posSaver;

    delete this->blockInt;

    this->gui->removeWindow(this->inventoryUi);
    this->inventoryUi = nullptr;
    delete this->inventory;

    this->lighting = nullptr;
    this->hdr = nullptr;
    this->fxaa = nullptr;

    gSceneRenderer = nullptr;
    this->steps.clear();

    delete this->physics;

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
    if(this->debugger) {
        this->debugger->draw();
    }

    // increment time (XXX: make this more accurate)
    if(!this->paused) {
        // @60fps: 60 sec * 24 min
        this->time += 1./(3600. * 24);
    }
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
