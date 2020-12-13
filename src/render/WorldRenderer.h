#ifndef RENDER_WORLDRENDERER_H
#define RENDER_WORLDRENDERER_H

#include "gui/RunLoopStep.h"
#include "Camera.h"

#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <vector>

namespace input {
    class InputManager;
}

namespace render {
class RenderStep;
class Lighting;
class HDR;
class FXAA;

class WorldRenderer: public gui::RunLoopStep {
    public:
        WorldRenderer();
        virtual ~WorldRenderer();

    public:
        void willBeginFrame();
        void draw();

        void reshape(unsigned int width, unsigned int height);
        bool handleEvent(const SDL_Event &);

        /// returns a vector of (zNear, zFar) for clipping
        glm::vec2 getZPlane(void) const {
            return glm::vec2(this->zNear, this->zFar);
        }
        /// gets a const reference to the camera
        const Camera &getCamera() const {
            return this->camera;
        }

    private:
        void updateView();

    private:
        // used for keyboard/game controller input
        std::shared_ptr<input::InputManager> input = nullptr;

    private:
        // near and far clipping planes
        float zNear = 0.1f;
        float zFar = 100.f;
        // field of view, in degrees
        float projFoV = 50.f;
        // size of the viewport (render canvas)
        unsigned int viewportWidth = 0, viewportHeight = 0;

        // projection matrix
        glm::mat4 projection;

        // camera
        Camera camera;

        // render stages
        std::vector<std::shared_ptr<RenderStep>> steps;

        std::shared_ptr<Lighting> lighting = nullptr;
        std::shared_ptr<HDR> hdr = nullptr;
        std::shared_ptr<FXAA> fxaa = nullptr;
};
}

#endif
