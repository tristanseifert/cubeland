/**
 * Renders the particles from all active particle systems.
 */
#ifndef PARTICLES_RENDERER_H
#define PARTICLES_RENDERER_H

#include <mutex>
#include <vector>
#include <memory>
#include <cstddef>

#include <glbinding/gl/types.h>

#include "render/RenderStep.h"

struct MetricsGuiMetric;
struct MetricsGuiPlot;

namespace physics {
class Engine;
}

namespace gfx {
class Buffer;
class VertexArray;
class ShaderProgram;
class Texture2D;
}

namespace particles {
class System;

class Renderer: public render::RenderStep {
    friend class System;

    public:
        Renderer();
        ~Renderer();

        virtual void startOfFrame() override;

        virtual void preRender(render::WorldRenderer *) override;
        virtual void render(render::WorldRenderer *) override;
        virtual void postRender(render::WorldRenderer *) override;

        virtual void reshape(int, int) override {};

        // Do not care about G buffer
        virtual const bool requiresBoundGBuffer() override {
            return false;
        }
        // Render directly into the HDR input buffer. This has depth attached.
        virtual const bool requiresBoundHDRBuffer() override {
            return true;
        }

        /// Sets the physics engine pointer.
        void setPhysicsEngine(physics::Engine *newEngine) {
            this->phys = newEngine;
        }

        /// Adds a new particle system
        void addSystem(std::shared_ptr<System> &system);
        /// Removes an existing particle system
        void removeSystem(std::shared_ptr<System> &system);

    private:
        struct ParticleInfo {
            glm::vec3 pos;
            glm::vec3 color = glm::vec3(1);
            glm::vec4 uv;
            float scale = 1.;
            float alpha = 1.;
        };

    private:
        static const gl::GLfloat kQuadData[];

    private:
        void rebuildAtlas();

        void drawDebugWindow();

    private:
        physics::Engine *phys = nullptr;

        gfx::ShaderProgram *shader = nullptr;
        gfx::Buffer *quadVtxBuf = nullptr;
        gfx::Buffer *particleInfoBuf = nullptr;
        gfx::VertexArray *particleVao = nullptr;
        gfx::Texture2D *particleAtlas = nullptr;

        size_t numParticles = 0;

        /// lock protecting particle info list
        std::mutex particleInfoLock;
        /// info on all particles
        std::vector<ParticleInfo> particleInfo;
        /// when set, the particle buffer is yeeted to the GPU next frame
        bool particleInfoDirty = false;

        /// lock over particle systems
        std::mutex particleSystemsLock;
        /// all particle systems in the world
        std::vector<std::shared_ptr<System>> particleSystems;

        MetricsGuiPlot *mPlot;
        MetricsGuiMetric *mNumParticles, *mVisibleSystems;
};
}

#endif
