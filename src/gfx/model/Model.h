/**
 * Provides support for loading .obj files via assimp.
 */
#ifndef GFX_MODEL_MODEL_H
#define GFX_MODEL_MODEL_H

#include "Mesh.h"
#include "render/scene/Drawable.h"

#include <string>
#include <memory>
#include <vector>

namespace gfx {
class RenderProgram;

class Model: public render::Drawable {
    public:
        Model(const std::string &path);

        void draw(std::shared_ptr<RenderProgram> program);

    private:
        void parseModel(const std::string &, const std::string &);

    private:
        // meshes this model is composed of
        std::vector<Mesh> meshes;
        // textures this model uses
        std::vector<TextureStruct> texturesLoaded;

        // base directory the model is in
        std::string modelBase;
};
}

#endif
