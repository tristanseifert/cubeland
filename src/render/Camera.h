#ifndef RENDER_CAMERA_H
#define RENDER_CAMERA_H

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace render {
class Camera {
    public:
        Camera();

        void updateAngles(const glm::vec3 &euler, const glm::vec3 &eulerNoPitch);
        void updatePosition(const glm::vec3 &deltas);
        glm::vec3 deltasToPos(const glm::vec3 &deltas);

        void updateViewMatrix();

        const glm::mat4 getViewMatrix(void) const {
            return this->view;
        }

        const glm::vec3 getShiftedCameraPosition(void) const {
            return this->cameraPosition + glm::vec3(0, this->yOffset, 0);
        }
        const glm::vec3 getCameraPosition(void) const {
            return this->cameraPosition;
        }
        const glm::vec3 getCameraFront(void) const {
            return this->cameraFront;
        }
        const glm::vec3 getCameraLookAt(void) const {
            return this->cameraLookAt;
        }
        const glm::vec3 getCameraUp(void) const {
            return this->up;
        }

        const float getYOffset() const {
            return this->yOffset;
        }

        /// Applies a raw vector offset to the camera position.
        void applyRawDeltas(const glm::vec3 &deltas);

        /// Sets the camera Y offset
        void setCameraYOffset(const float newOffset) {
            this->yOffset = newOffset;
        }

        /// Sets the position of the camera
        void setCameraPosition(glm::vec3 position) {
            this->cameraPosition = position;
        }

        void startFrame();

    private:
        void drawDebugWindow();

    private:
        glm::vec3 cameraPosition;
        glm::vec3 cameraFront, cameraFrontNoPitch;
        glm::vec3 cameraLookAt;

        glm::vec3 up;
        glm::vec3 right;
        glm::vec3 worldUp;

        glm::mat4 view;

        // Y offset of the actual view position
        float yOffset = 1.74f;

        // when set, the camera debug window is visible
        bool showDebugWindow = false;
};
}

#endif
