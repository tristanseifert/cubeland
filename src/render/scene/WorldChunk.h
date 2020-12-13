/**
 * Responsible for drawing a single chunk (a pile of blocks) of the world.
 */
#ifndef RENDER_SCENE_WORLDCHUNK_H
#define RENDER_SCENE_WORLDCHUNK_H

#include "Drawable.h"

#include <memory>

namespace gfx {
class RenderProgram;

class VertexArray;
class Buffer;
}

namespace render {
class WorldChunk: public Drawable {
    public:
        WorldChunk();

        virtual void draw(std::shared_ptr<gfx::RenderProgram> program);

    private:
        // vertex array and buffer for a single cube
        std::shared_ptr<gfx::VertexArray> vao = nullptr;
        std::shared_ptr<gfx::Buffer> vbo = nullptr;

};
}

#endif
