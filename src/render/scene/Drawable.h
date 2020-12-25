/**
 * Defines the interface of an object that's drawable by the scene renderer.
 */
#ifndef RENDER_SCENE_DRAWABLE_H
#define RENDER_SCENE_DRAWABLE_H

#include <memory>

namespace gfx {
class RenderProgram;
}

namespace render {
class Drawable {
    public:
        virtual void draw(std::shared_ptr<gfx::RenderProgram> &program) = 0;

        virtual void frameBegin() {};
};
}

#endif
