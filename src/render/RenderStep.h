/**
 * Implements an abstract interface that all world render steps implement.
 */
#ifndef RENDER_RENDERSTEP_H
#define RENDER_RENDERSTEP_H

#include <string>
#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace render {
class WorldRenderer;
class WorldRendererDebugger;

class RenderStep {
    friend class Renderer;
    friend class WorldRendererDebugger;

    public:
        RenderStep() = default;
        RenderStep(const std::string &category, const std::string &title);
        virtual ~RenderStep();

        virtual void startOfFrame() {};

        virtual void preRender(WorldRenderer *) = 0;
        virtual void render(WorldRenderer *) = 0;
        virtual void postRender(WorldRenderer *) = 0;

        virtual const bool requiresBoundGBuffer() = 0;
        virtual const bool requiresBoundHDRBuffer() = 0;

        // update the size of the output render area, in device pixels
        virtual void reshape(int w, int h) = 0;

    public:
        glm::mat4 projectionMatrix;

        glm::vec2 viewportSize;

        glm::mat4 viewMatrix;
        glm::vec3 viewPosition; // camera position
        glm::vec3 viewLookAt; // camera "look at" vector
        glm::vec3 viewDirection; // camera front vector
        glm::vec3 viewUp; // camera up vector

    protected:
        bool showDebugWindow = false;

    private:
        uint32_t debugMenuItem = 0;
};
}

#endif
