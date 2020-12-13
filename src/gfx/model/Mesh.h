/**
 * Encapsulates a collection of vertices that are a part of a model.
 */
#ifndef GFX_MODEL_MESH_H
#define GFX_MODEL_MESH_H

#include "render/scene/Drawable.h"

#include <string>
#include <memory>
#include <vector>

#include <glbinding/gl/gl.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace gfx {
class RenderProgram;
class Texture;
class Buffer;
class VertexArray;

/**
 * A single vertex in a mesh, encompassing a position, normal, and texture coordinates.
 */
struct VertexStruct {
    // Position
    glm::vec3 Position;
    // Normal
    glm::vec3 Normal;
    // TexCoords
    glm::vec2 TexCoords;
};

/**
 * Each mesh can have textures associated with it; this serves as a small encapsulating data
 * container that maps the mesh texture IDs to an actual loaded texture object.
 */
struct TextureStruct {
    std::shared_ptr<Texture> tex = nullptr;

    std::string type;
    // aiString path;
};

class Mesh: public render::Drawable {
    public:
        Mesh(const std::vector<VertexStruct> &vertices, const std::vector<gl::GLuint> &indices,
             const std::vector<TextureStruct> &textures);

        void draw(std::shared_ptr<RenderProgram> program);


    private:
        void prepareMesh(void);

    private:
        std::vector<VertexStruct> vertices;
        std::vector<gl::GLuint> indices;
        std::vector<TextureStruct> textures;

        std::shared_ptr<VertexArray> vao = nullptr;
        std::shared_ptr<Buffer> vbo = nullptr;
        std::shared_ptr<Buffer> ebo = nullptr;

};
}

#endif
