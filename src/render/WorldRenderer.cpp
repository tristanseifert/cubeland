#include "WorldRenderer.h"
#include "RenderStep.h"
#include "input/InputManager.h"

#include "steps/FXAA.h"

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
}
/**
 * Releases all of our render resources.
 */
WorldRenderer::~WorldRenderer() {

}

/**
 * Prepare the world for rendering.
 */
void WorldRenderer::willBeginFrame() {
    this->input->startFrame();
    this->updateView();
}

/**
 * Handle the drawing stages.
 */
void WorldRenderer::draw() {

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
