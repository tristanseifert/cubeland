#include "WorldChunk.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>

using namespace render;

/**
 * Fixed vertices (x, y, z), normals (xyz) and (uv) for a cube that's one unit in each dimension.
 */
static const gl::GLfloat kCubeVertices[] = {
    // back face
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,   0.0f, 0.0f,

    // front face
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,    0.0f, 1.0f,

    // left face
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,

    // right face
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,   1.0f, 0.0f,

     // bottom face
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,   0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,   0.0f, 0.0f,

    // top face
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,   0.0f, 1.0f
};

/**
 * Sets up the static buffers used to draw the blocks in the world.
 */
WorldChunk::WorldChunk() {
    using namespace gfx;

    // create the buffers of cube vertices
    this->vao = std::make_shared<VertexArray>();
    this->vbo = std::make_shared<Buffer>(Buffer::Array, Buffer::StaticDraw);

    // bind VAO so all changes go to it
    this->vao->bind();

    // fill the vertex buffer with our vertex structs
    this->vbo->bind();
    this->vbo->bufferData(sizeof(kCubeVertices), (void *) &kCubeVertices);

    // index of vertex position
    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat), 0);
    // normals
    this->vao->registerVertexAttribPointer(1, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat));
    // index of texture sampling position
    this->vao->registerVertexAttribPointer(2, 2, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            6 * sizeof(gl::GLfloat));

    // unbind the VAO
    VertexArray::unbind();

    // lastly, load the placeholder texture
    this->placeholderTex = std::make_shared<Texture2D>();
    this->placeholderTex->loadFromImage("/test/empty/whitegreen.png");
}

/**
 * Uses instanced rendering to draw the blocks of the chunk.
 *
 * At this point, our draw list should have been culled to the point that only blocks exposed to
 * air (e.g. ones that could be visible) are in it.
 */
void WorldChunk::draw(std::shared_ptr<gfx::RenderProgram> program) {
    // bind diffuse/specular textures, if needed
    if(program->rendersColor()) {
        this->placeholderTex->bind();
        program->setUniform1i("texture_diffuse1", this->placeholderTex->unit);

        // set the shininess and how many diffuse/specular textures we have
        program->setUniform1f("Material.shininess", 16.f);

        glm::vec2 texNums(1, 0);
        program->setUniformVec("NumTextures", texNums);
    }

    // render
    this->vao->bind();

    gl::glDrawArrays(gl::GL_TRIANGLES, 0, 36);

    gfx::VertexArray::unbind();
}

