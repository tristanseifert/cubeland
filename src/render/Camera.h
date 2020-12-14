#ifndef RENDER_CAMERA_H
#define RENDER_CAMERA_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace render {
class Camera {
    public:
        Camera();

        void updateViewMatrix(const glm::vec3 &euler, float xDelta, float zDelta, float yDelta = 0);

        glm::mat4 getViewMatrix(void) const {
            return this->view;
        }

        glm::vec3 getCameraPosition(void) const {
            return this->camera_position;
        }
        glm::vec3 getCameraFront(void) const {
            return this->camera_front;
        }
        glm::vec3 getCameraLookAt(void) const {
            return this->camera_look_at;
        }
        glm::vec3 getCameraUp(void) const {
            return this->up;
        }

        void setCameraPosition(glm::vec3 position) {
            this->camera_position = position;
        }

        void startFrame();

    private:
        void drawDebugWindow();

    private:
        glm::vec3 camera_position;
        glm::vec3 camera_front;
        glm::vec3 camera_look_at;

        glm::vec3 up;
        glm::vec3 right;
        glm::vec3 world_up;

        glm::mat4 view;

        // when set, the camera debug window is visible
        bool showDebugWindow = false;
};
}

#endif
