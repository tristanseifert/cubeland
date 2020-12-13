#include "WorldRenderer.h"
#include "RenderStep.h"
#include "scene/SceneRenderer.h"
#include "input/InputManager.h"

#include "steps/FXAA.h"
#include "steps/Lighting.h"
#include "steps/HDR.h"

#include "gfx/gl/buffer/FrameBuffer.h"

#include <Logging.h>

#include <glm/ext.hpp>
#include <SDL.h>

using namespace render;

/**
 * Creates the renderer resources.
 */
WorldRenderer::WorldRenderer() {
    // create the IO manager
    this->input = std::make_shared<input::InputManager>();

    // then, the render steps
    auto scnRnd = std::make_shared<SceneRenderer>();
    this->steps.push_back(scnRnd);

    this->lighting = std::make_shared<Lighting>();
    this->steps.push_back(this->lighting);

    this->hdr = std::make_shared<HDR>();
    this->steps.push_back(this->hdr);

    this->fxaa = std::make_shared<FXAA>();
    this->steps.push_back(this->fxaa);

    // Set up some shared buffers
    this->lighting->setSceneRenderer(scnRnd);

    this->hdr->setDepthBuffer(this->lighting->gDepth);
    this->hdr->setOutputFBO(this->fxaa->getFXAABuffer());
}
/**
 * Releases all of our render resources.
 */
WorldRenderer::~WorldRenderer() {
    this->steps.clear();
}

/**
 * Prepare the world for rendering.
 */
void WorldRenderer::willBeginFrame() {
    this->input->startFrame();
    this->camera.startFrame();

    this->updateView();

    for(auto &step : this->steps) {
        step->startOfFrame();
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
    // send it to the IO handler
    return this->input->handleEvent(event);
    // return false;
}



/**
 * Updates the world view matrices.
 */
void WorldRenderer::updateView() {
    // update camera with Euler angles
    glm::vec3 angles = this->input->getEulerAngles();
    glm::vec3 deltas = this->input->getMovementDelta();

    this->camera.updateViewMatrix(angles, deltas.x, deltas.z, deltas.y);

    // calculate projection matrix
    float width = (float) this->viewportWidth;
    float height = (float) this->viewportHeight;

    this->projection = glm::perspective(this->projFoV, (float) (width / height), this->zNear,
                                        this->zFar);


    // give each of the stages some information from the camera for rendering
    for(auto stage : this->steps) {
        stage->viewMatrix = this->camera.getViewMatrix();
        stage->viewPosition = this->camera.getCameraPosition();
        stage->viewLookAt = this->camera.getCameraLookAt();
        stage->viewDirection = this->camera.getCameraFront();
        stage->viewUp = this->camera.getCameraUp();

        stage->projectionMatrix = this->projection;
    }
}
