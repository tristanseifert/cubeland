/**
 * Renders the particles from all active particle systems.
 */
#ifndef PARTICLES_RENDERER_H
#define PARTICLES_RENDERER_H

#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <cstddef>

#include <glbinding/gl/types.h>

#include "render/RenderStep.h"
#include "util/TexturePacker.h"

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

        /// Loads the given texture into the particle image atlas.
        bool addTexture(const glm::ivec2 &size, const std::string &path);
        /// Gets the texture atlas UV of the given texture.
        glm::vec4 getUv(const std::string &path) {
            return this->texturesPacker.uvBoundsForTexture(path);
        }

    private:
        struct ParticleInfo {
            glm::vec3 pos;
            glm::vec3 color = glm::vec3(1);
            glm::vec4 uv = glm::vec4(0, 0, 1, 1);
            float scale = 1.;
            float alpha = 1.;
        };

        struct TextureInfo {
            /// resource directory path
            std::string path;
            /// size of the texture
            glm::ivec2 size;
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
        /// when not set, we skip the drawing process entirely
        bool hasVisibleSystems = false;

        /// lock over particle systems
        std::mutex particleSystemsLock;
        /// all particle systems in the world
        std::vector<std::shared_ptr<System>> particleSystems;

        /// currently loaded textures
        std::unordered_map<std::string, TextureInfo> textures;
        /// lock protecting this map
        std::mutex texturesLock;
        /// texture packer for these textures
        util::TexturePacker<std::string> texturesPacker;
        /// whether we need to update the texture atlas
        std::atomic_bool needsAtlasUpdate = false;

        MetricsGuiPlot *mPlot;
        MetricsGuiMetric *mNumParticles, *mVisibleSystems;
};
}

#endif
