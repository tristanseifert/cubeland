#ifndef GFX_MODEL_RENDERPROGRAM_H
#define GFX_MODEL_RENDERPROGRAM_H

#include "gfx/gl/program/ShaderProgram.h"

#include <memory>
#include <string>

namespace gfx {
class RenderProgram: public ShaderProgram {
    public:
        RenderProgram(std::string vert, std::string frag, bool color = true) 
            : ShaderProgram(vert, frag), hasColor(color) { }
        virtual ~RenderProgram() {};

        // whether this program is used for rendering to color attachments
        bool rendersColor(void) const {
            return this->hasColor;
        }

    protected:
        bool hasColor;
};
}

#endif
