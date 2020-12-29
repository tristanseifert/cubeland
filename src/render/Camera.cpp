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
    this->cameraPosition = glm::vec3(-10, 100, 0);
    this->cameraFront = glm::vec3(-0.689, -0.022, 0.724);
    this->worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
}

/**
 * Updates the angles used by the camera to determine which direction to look.
 */
void Camera::updateAngles(const glm::vec3 &euler, const glm::vec3 &eulerNoPitch) {
    // Update the camera front vector
    this->cameraFront = normalize(euler);
    this->cameraFrontNoPitch = normalize(eulerNoPitch);

    // Also recalculate the Up and Right vectors
    this->right = normalize(cross(this->cameraFront, this->worldUp));
    this->up = normalize(cross(this->right, this->cameraFront));
}

/**
 * Returns the new camera position based on the given deltas. This doesn't actually perform any
 * changes yet.
 */
glm::vec3 Camera::deltasToPos(const glm::vec3 &deltas) {
    auto position = this->cameraPosition;

    const auto nRight = normalize(cross(this->cameraFront, this->worldUp));
    const auto nUp = normalize(cross(this->right, this->cameraFront));

    position += nRight * deltas.x;
    position += this->cameraFrontNoPitch * deltas.z;
    // position += this->cameraFront * deltas.z;
    position.y += deltas.y;

    return position;
}

/**
 * Updates the camera position.
 */
void Camera::updatePosition(const glm::vec3 &deltas) {
    this->cameraPosition = this->deltasToPos(deltas);
}

/**
 * Updates the raw deltas directly against the position vector.
 */
void Camera::applyRawDeltas(const glm::vec3 &deltas) {
    this->cameraPosition += deltas;
}

/**
 * Recalculates the view matrix.
 */
void Camera::updateViewMatrix() {
    const auto shiftedPos = this->cameraPosition + glm::vec3(0, this->yOffset, 0);
    this->cameraLookAt = shiftedPos + this->cameraFront;

    this->view = glm::lookAt(shiftedPos, this->cameraLookAt, this->up);
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

    ImGui::DragFloat3("Position", &this->cameraPosition.x);
    ImGui::DragFloat3("Front", &this->cameraFront.x);
    ImGui::DragFloat3("Look-at", &this->cameraLookAt.x);

    ImGui::PopItemWidth();

done:;
    ImGui::End();
}
