#include "Mesh.h"
#include "RenderProgram.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture.h"

#include <string>
#include <sstream>

using namespace gfx;

/**
 * Initializes the mesh and assigns texture units.
 */
Mesh::Mesh(const std::vector<VertexStruct> &vertices, const std::vector<gl::GLuint> &indices,
           const std::vector<TextureStruct> &textures) {
    // store the data
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;

    // set up the textures to use sequential units
    int unit = 0;
    for(auto &texture : this->textures) {
        texture.tex->unit = unit++;
    }

    // prepare the mesh for drawing
    this->prepareMesh();
}

/**
 * Prepares the mesh for drawing, by allocating the vertex array and buffers,
 * then filling the appropriate data into them.
 */
void Mesh::prepareMesh(void) {
    // allcoate the buffers
    this->vao = std::make_shared<VertexArray>();

    this->vbo = std::make_shared<Buffer>(Buffer::Array, Buffer::StaticDraw);
    this->ebo = std::make_shared<Buffer>(Buffer::ElementArray, Buffer::StaticDraw);

    // bind VAO so all changes go to it
    this->vao->bind();

    // fill the vertex buffer with our vertex structs
    this->vbo->bind();
    // this->vbo->bufferData(this->vertices.size() * sizeof(VertexStruct), (void *) &(this->vertices[0]));
    this->vbo->bufferData(this->vertices.size() * sizeof(VertexStruct), this->vertices.data());

    // fill the element buffer with the indices
    this->ebo->bind();
    // this->ebo->bufferData(this->indices.size() * sizeof(gl::GLuint), (void *) &(this->indices[0]));
    this->ebo->bufferData(this->indices.size() * sizeof(gl::GLuint), this->indices.data());

    // configure the vertex attributes (position, normal, UV coords)
    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, sizeof(VertexStruct),
                                           offsetof(VertexStruct, Position));

    this->vao->registerVertexAttribPointer(1, 3, VertexArray::Float, sizeof(VertexStruct),
                                           offsetof(VertexStruct, Normal));

    this->vao->registerVertexAttribPointer(2, 2, VertexArray::Float, sizeof(VertexStruct),
                                           offsetof(VertexStruct, TexCoords));

    // unbind the VAO
    VertexArray::unbind();
}

/**
 * Binds all the necessary buffers, binds the textures to the uniforms on the
 * model shader, then issues the draw call.
 *
 * @note This expects that "model.shader," or a variant thereof, is bound.
 */
void Mesh::draw(std::shared_ptr<RenderProgram> program) {
    // only set up textures and whatnot if this program has color rendering
    if(program->rendersColor() == true) {
        // bind the textures
        int diffuseNr = 1;
        int specularNr = 1;

        for(unsigned int i = 0; i < this->textures.size(); i++) {
            // acquire the texture
            std::stringstream ss;
            std::string number;
            std::string name = this->textures[i].type;

            if(name == "texture_diffuse") {
                ss << diffuseNr++;
            } else if(name == "texture_specular") {
                ss << specularNr++;
            }

            number = ss.str();

            // bind the texture
            program->setUniform1i((name + number), i);
            this->textures[i].tex->bind();
        }

        // set the shininess and how many diffuse/specular textures we have
        program->setUniform1f("Material.shininess", 32.f);

        glm::vec2 texNums(diffuseNr - 1, specularNr - 1);
        program->setUniformVec("NumTextures", texNums);
    }

    // draw
    this->vao->bind();

    glDrawElements(gl::GL_TRIANGLES, (gl::GLsizei) this->indices.size(), gl::GL_UNSIGNED_INT, 0);

    VertexArray::unbind();
}
