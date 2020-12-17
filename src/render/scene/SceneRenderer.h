#ifndef RENDER_SCENE_SCENERENDERER_H
#define RENDER_SCENE_SCENERENDERER_H

#include "../RenderStep.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>

namespace gfx {
class RenderProgram;
}

namespace world {
class WorldDebugger;
}

namespace render {
class Drawable;
class WorldChunk;

class SceneRenderer: public RenderStep {
    friend class Lighting;
    friend class world::WorldDebugger;

    public:
        SceneRenderer();
        virtual ~SceneRenderer();

        void startOfFrame();

        void preRender(WorldRenderer *);
        void render(WorldRenderer *renderer);
        void postRender(WorldRenderer *);

        const bool requiresBoundGBuffer() { return true; }
        const bool requiresBoundHDRBuffer() { return false; }

        // ignored
        void reshape(int w, int h) {};

    protected:
        void render(glm::mat4 projView, const bool shadow = false, const bool hasNormalMatrix = true);

    private:
        void prepareChunk(std::shared_ptr<gfx::RenderProgram>, std::shared_ptr<WorldChunk>, bool);

    private:
        enum ProgramType {
            // drawing of chunks
            kProgramChunkDraw,
            // chunk outlines/selection
            kProgramChunkHighlight,
        };

    private:
        /// gets the appropriate program from either the shadow or color program section
        std::shared_ptr<gfx::RenderProgram> getProgram(const ProgramType type, const bool shadow) {
            if(shadow) {
                return this->shadowPrograms[type];
            } else {
                return this->colorPrograms[type];
            }
        }

    private:
        /// the chunks we're rendering as part of the world
        std::vector<std::shared_ptr<WorldChunk>> chunks;

        float time = 0;

        /// regular (color rendering) programs
        std::unordered_map<ProgramType, std::shared_ptr<gfx::RenderProgram>> colorPrograms;
        /// shadow rendering programs
        std::unordered_map<ProgramType, std::shared_ptr<gfx::RenderProgram>> shadowPrograms;
};
}

#endif
