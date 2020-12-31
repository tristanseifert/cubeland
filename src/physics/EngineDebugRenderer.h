/**
 * Provides an interface for rendering arbitrary lines and triangles as needed to display debug
 * info from the physics engine.
 */
#ifndef PHYSICS_ENGINEDEBUGRENDERER_H
#define PHYSICS_ENGINEDEBUGRENDERER_H

#include "render/RenderStep.h"

namespace reactphysics3d {
class PhysicsWorld;
}

namespace gfx {
class Buffer;
class VertexArray;
class ShaderProgram;
}

namespace physics {
class Engine;

class EngineDebugRenderer: public render::RenderStep {
    friend class Engine;

    public:
        EngineDebugRenderer();
        virtual ~EngineDebugRenderer();

        virtual void preRender(render::WorldRenderer *) override;
        virtual void render(render::WorldRenderer *) override;
        virtual void postRender(render::WorldRenderer *) override {};

        virtual void reshape(int, int) override {};

        // Render to G buffer
        virtual const bool requiresBoundGBuffer() override {
            return true;
        }
        // Do not care about the HDR buffer.
        virtual const bool requiresBoundHDRBuffer() override {
            return false;
        }

        /// Sets whether the debug data is shown or not.
        void setDrawsDebugData(const bool newValue) {
            this->drawDebugData = newValue;
        }

    private:
        /// line/point drawing shader
        gfx::ShaderProgram *shader = nullptr;

        /// vertex array for lines
        gfx::VertexArray *lineVao = nullptr;
        /// buffer holding line vertex data
        gfx::Buffer *lineVbo = nullptr;

        /// vertex array for triangles
        gfx::VertexArray *triangleVao = nullptr;
        /// buffer holding triangle vertex data
        gfx::Buffer *triangleVbo = nullptr;

        /// physics world from which to take the debug data
        reactphysics3d::PhysicsWorld *world = nullptr;
        /// whether we bother with displaying debug data or not
        bool drawDebugData = false;
};
}

#endif
