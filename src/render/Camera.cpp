#include "Camera.h"

#include <Logging.h>
#include "io/Format.h"

#include <imgui.h>
#include <glm/ext.hpp>

using namespace render;

/**
 * Sets up default camera parameters.
 */
Camera::Camera() {
    // default camera position
    // this->camera_position = glm::vec3(15.6459, 25.0754, -14.6763);
    // this->camera_front = glm::vec3(-0.99, 0.06, -0.14);
    
    this->camera_position = glm::vec3(23.5, -12, 24);
    this->camera_front = glm::vec3(-0.62, 0.209, -0.756);
    this->world_up = glm::vec3(0.0f, 1.0f, 0.0f);
}

/**
 * Updates the camera matrix, based on the camera position, and the "look at" vector, given the
 * Euler angles, and X/Z delta from the last invocation.
 */
void Camera::updateViewMatrix(const glm::vec3 &euler, float xDelta, float zDelta, float yDelta) {
    // Update the camera front vector
    this->camera_front = normalize(euler);

    // Also recalculate the Up and Right vectors
    this->right = normalize(cross(this->camera_front, this->world_up));
    this->up = normalize(cross(this->right, this->camera_front));

    if(xDelta != 0.f) {
        this->camera_position += this->right * xDelta;
    }

    if(zDelta != 0.f) {
        this->camera_position += this->camera_front * zDelta;
    }

    // Ensure the camera stays at ground level
    this->camera_position.y += yDelta;

    // Re-calculate the look-at vector
    this->camera_look_at = this->camera_position + this->camera_front;

    // recalculate the view matrix
    this->view = glm::lookAt(this->camera_position, this->camera_look_at, this->up);
}

/**
 * Indicates a new frame has begun. Primarily used for debug ui.
 */
void Camera::startFrame() {
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
}
/**
 * Draws the debug window
 */
void Camera::drawDebugWindow() {
    // short circuit drawing if not visible
    if(!ImGui::Begin("Camera", &this->showDebugWindow, ImGuiWindowFlags_NoResize)) {
        goto done;
    }

    ImGui::PushItemWidth(225);

    ImGui::DragFloat3("Position", &this->camera_position.x);
    ImGui::DragFloat3("Front", &this->camera_front.x);
    ImGui::DragFloat3("Look-at", &this->camera_look_at.x);

    ImGui::PopItemWidth();

done:;
    ImGui::End();
}
