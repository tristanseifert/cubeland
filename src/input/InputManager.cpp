#include "InputManager.h"
#include "gfx/gl/texture/TextureDumper.h"

#include <Logging.h>
#include "io/Format.h"

#include <imgui.h>
#include <glm/vec3.hpp>
#include <SDL.h>

#include <algorithm>

using namespace input;

/**
 * Initializes the input manager./
 */
InputManager::InputManager() {
    this->keys.reset();
}

/**
 * At the start of the frame, zero out our angles based on the previous frame's input movement.
 */
void InputManager::startFrame() {
    // calculate angles and positions
    this->updateAngles();
    this->updatePosition();

    // reset deltas
    this->mouseDeltaX = this->mouseDeltaY = 0;
    this->moveDeltaX = this->moveDeltaY = this->moveDeltaZ = 0;

    // draw the UI
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
}

/**
 * Updates the Euler angles based on the mouse movements.
 */
void InputManager::updateAngles() {
    // calculate pitch and yaw from the mouse deltas
    float xOffset = this->mouseDeltaX * this->lookSensitivity;
    float yOffset = this->mouseDeltaY * this->lookSensitivity;

    yOffset *= (this->reverseLookUpDown) ? 1. : -1.;

    this->yaw += xOffset;
    this->pitch += yOffset;

    // Limit the yaw and pitch.
    this->pitch = std::min(this->pitch, 89.f);
    this->pitch = std::max(this->pitch, -89.f);

    // update the Euler angles
    this->eulerAngles.x = cos(glm::radians(this->pitch)) * cos(glm::radians(this->yaw));
    this->eulerAngles.y = sin(glm::radians(this->pitch));
    this->eulerAngles.z = cos(glm::radians(this->pitch)) * sin(glm::radians(this->yaw));
}

/**
 * Updates the position.
 */
void InputManager::updatePosition() {
    // interpret the keys
    if(this->keys[KeyMoveLeft]) {
        this->moveDeltaX = -1.f;
    } else if(this->keys[KeyMoveRight]) {
        this->moveDeltaX = 1.f;
    }

    if(this->keys[KeyMoveFront]) {
        this->moveDeltaZ = 1.f;
    } if(this->keys[KeyMoveBack]) {
        this->moveDeltaZ = -1.f;
    }


    if(this->keys[KeyMoveUp]) {
        this->moveDeltaY = 1.f;
    } if(this->keys[KeyMoveDown]) {
        this->moveDeltaY = -1.f;
    }

    // calculate the X and Z offsets
    float xOffset = this->moveDeltaX * this->movementSensitivity;
    float yOffset = this->moveDeltaY * this->movementSensitivity;
    float zOffset = this->moveDeltaZ * this->movementSensitivity;

    // create a vector
    this->movementDelta = glm::vec3(xOffset, yOffset, zOffset);
}

/**
 * Handles SDL events.
 *
 * Currently, we capture all keyboard and mouse movement events.
 */
bool InputManager::handleEvent(const SDL_Event &event) {

    switch(event.type) {
        // mouse moved
        case SDL_MOUSEMOTION:
            if(this->inputUpdatesCamera) {
                this->mouseDeltaX = (double) event.motion.xrel;
                this->mouseDeltaY = (double) event.motion.yrel;
            }
            break;

        // key down
        case SDL_KEYDOWN:
            this->handleKey(event.key.keysym.scancode, event.key.keysym.mod, true);
            break;
        // key up
        case SDL_KEYUP:
            this->handleKey(event.key.keysym.scancode, event.key.keysym.mod, false);
            break;

        // unhandled event
        default:
            break;
    }

    // if we get here, the event was unhandled and should be propagated
    return false;
}

/**
 * Handles an SDL keyboard event, taking the scancode, modifier state, and up/down state.
 */
void InputManager::handleKey(int scancode, unsigned int modifiers, bool isDown) {
    // these keys are always handled
  switch(scancode) {
        // pressing "P" will save all the textures
        case SDL_SCANCODE_P:
            if(isDown) {
                gfx::TextureDumper::sharedDumper()->dump();
            }
            break;

        // F6 toggles the accepts user input flag if debug window is open
        case SDL_SCANCODE_F6:
            if(this->showDebugWindow && !isDown) {
                this->inputUpdatesCamera = !this->inputUpdatesCamera;
            }
            break;

        default:
            break;
    }
    // only handled for user interaction
    if(!this->inputUpdatesCamera) return;

    switch(scancode) {
        case SDL_SCANCODE_W:
            this->keys[KeyMoveFront] = isDown;
            break;

        case SDL_SCANCODE_S:
            this->keys[KeyMoveBack] = isDown;
            break;

        case SDL_SCANCODE_A:
            this->keys[KeyMoveLeft] = isDown;
            break;

        case SDL_SCANCODE_D:
            this->keys[KeyMoveRight] = isDown;
            break;


        case SDL_SCANCODE_UP:
            this->keys[KeyMoveUp] = isDown;
            break;

        case SDL_SCANCODE_DOWN:
            this->keys[KeyMoveDown] = isDown;
            break;

        default:
            break;
    }
}



/**
 * Draws the input manager debug window
 */
void InputManager::drawDebugWindow() {
    // short circuit drawing if not visible
    if(!ImGui::Begin("Input Manager", &this->showDebugWindow, ImGuiWindowFlags_NoResize)) {
        goto done;
    }

    // various checkboxes
    ImGui::Checkbox("Accept user input", &this->inputUpdatesCamera);
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Press F6 to toggle while this window is open");
    }

    ImGui::Checkbox("Reverses Y", &this->reverseLookUpDown);

    // pitch, yaw and roll
    ImGui::PushItemWidth(74);
    ImGui::DragFloat("Pitch", &this->pitch, 1, -90, 90);
    ImGui::DragFloat("Yaw", &this->yaw, 1);
    // ImGui::DragFloat("Roll", &this->roll, 1);

    // sensitivities
    ImGui::DragFloat("Look sensitivity", &this->lookSensitivity, 0);
    ImGui::DragFloat("Move sensitivity", &this->movementSensitivity, 0);

    ImGui::PopItemWidth();

done:;
    ImGui::End();
}
