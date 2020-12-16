#ifndef RENDER_SCENE_SCENERENDERER_H
#define RENDER_SCENE_SCENERENDERER_H

#include "../RenderStep.h"

#include <memory>

#include <glm/mat4x4.hpp>

namespace gfx {
class RenderProgram;
}

namespace world {
class WorldDebugger;
}

namespace render {
class Drawable;
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
        void _doRender(glm::mat4 projView, std::shared_ptr<gfx::RenderProgram> program, bool hasNormalMatrix = true);

    private:
        std::shared_ptr<gfx::RenderProgram> program;
        std::shared_ptr<Drawable> model;
};
}

#endif
