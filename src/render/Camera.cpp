#include "Camera.h"

#include <Logging.h>
#include "io/Format.h"

#include <glm/ext.hpp>

using namespace render;

/**
 * Sets up default camera parameters.
 */
Camera::Camera() {
    // default camera position
    this->camera_position = glm::vec3(15.6459, 25.0754, -14.6763);
    this->camera_front = glm::vec3(-0.99, 0.06, -0.14);

    // some defaults for rendering
    this->world_up = glm::vec3(0.0f, 2.0f, 0.0f);
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

    // debug
    /*cout << this->camera_position.x << ", " << this->camera_position.y << ", " <<
                    this->camera_position.z << endl;
    cout << euler.x << ", " << euler.y << ", " << euler.z << endl;*/
}
