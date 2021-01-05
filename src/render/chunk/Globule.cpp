#include "Globule.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/gl/buffer/VertexArray.h"

#include <Logging.h>
#include <mutils/time/profiler.h>

#include <glm/glm.hpp>

using namespace render::chunk;

/**
 * Initializes a new globule.
 *
 * This allocates the vertex and index buffers, configures a vertex array that can be used for
 * drawing the globule.
 */
Globule::Globule(WorldChunk *_c, const glm::ivec3 _pos) : position(_pos) {
    using namespace gfx;

    this->facesVao = new VertexArray;
}

/**
 * Wait for any pending work to complete.
 */
Globule::~Globule() {
    delete this->facesVao;
}

/**
 * Invalidates all buffers.
 */
void Globule::clearBuffers() {
    // also, inhibit drawing until we get a buffer assigned again
    this->numIndices = 0;
    this->numVertices = 0;

    this->inhibitDrawing = true;
}

/**
 * Sets the buffer to use for display rendering based on the vertex generator buffer struct. We
 * will take a copy of the buffer pointer and deallocate it as needed (shoutout ref counting)
 */
void Globule::setBuffer(const VertexGenerator::Buffer &buf) {
    using namespace gfx;
    using BlockVertex = VertexGenerator::BlockVertex;

    // re-prepare the VAO
    if(buf.numVertices) {
        this->facesVao->bind();
        buf.buffer->bind();

        const size_t kVertexSize = sizeof(BlockVertex);
        this->facesVao->registerVertexAttribPointerInt(0, 3, VertexArray::Short, kVertexSize,
                offsetof(BlockVertex, p)); // vertex position
        this->facesVao->registerVertexAttribPointerInt(1, 1, VertexArray::UnsignedShort, kVertexSize,
                offsetof(BlockVertex, blockId)); // block ID
        this->facesVao->registerVertexAttribPointerInt(2, 1, VertexArray::UnsignedByte, kVertexSize,
                offsetof(BlockVertex, face)); // face
        this->facesVao->registerVertexAttribPointerInt(3, 1, VertexArray::UnsignedByte, kVertexSize,
                offsetof(BlockVertex, vertexId)); // vertex id

        gfx::VertexArray::unbind();

        buf.buffer->unbind();

        this->vertexBuf = buf.buffer;
        this->numVertices = buf.numVertices;
    }

    // update index data
    if(buf.bytesPerIndex == 2) {
        this->indexFormat = gl::GL_UNSIGNED_SHORT;
    } else if(buf.bytesPerIndex == 4) {
        this->indexFormat = gl::GL_UNSIGNED_INT;
    } else {
        XASSERT(false, "Invalid index size: {}", buf.bytesPerIndex);
    }

    this->indexBuf = buf.indexBuffer;
    this->numIndices = buf.numIndices;

    // clear inhibition flags
    this->inhibitDrawing = false;
}


/**
 * Draws the globule.
 */
void Globule::draw(std::shared_ptr<gfx::RenderProgram> &program) {
    using namespace gl;

    // draw if we have indices to do so with
    if(this->isVisible && !this->inhibitDrawing && this->numIndices) {
        this->facesVao->bind();
        this->indexBuf->bind();

        glDrawElements(GL_TRIANGLES, this->numIndices, this->indexFormat, nullptr);

        gfx::VertexArray::unbind();
    }
}



/**
 * Fills the given texture with normal data for all faces a globule may secrete.
 *
 * For each face of the cube, we generate 4 vertices; this texture is laid out such that the face
 * index indexes into the Y coordinate, while the vertex index (0-3) indexes into the X coordinate;
 * that is to say, the texture is 4x6 in size.
 *
 * In the texture, the RGB component encodes the XYZ of the normal. The alpha component is set to
 * 1, but is not currently used.
 */
void Globule::fillNormalTex(gfx::Texture2D *tex) {
    using namespace gfx;

    std::vector<glm::vec4> data;
    data.resize(4 * 6, glm::vec4(0));

    // static normal data indexed by face
    static const glm::vec3 normals[6] = {
        // bottom
        glm::vec3(0, -1, 0),
        // top
        glm::vec3(0, 1, 0),
        // left
        glm::vec3(-1, 0, 0),
        // right
        glm::vec3(1, 0, 0),
        // Z-1
        glm::vec3(0, 0, -1),
        // Z+1
        glm::vec3(0, 0, 1),
    };

    for(size_t y = 0; y < 6; y++) {
        const size_t yOff = (y * 4);
        for(size_t x = 0; x < 4; x++) {
            data[yOff + x] = glm::vec4(normals[y], 1);
        }
    }

    // allocate texture data and send it
    tex->allocateBlank(4, 6, Texture2D::RGBA16F);
    tex->bufferSubData(4, 6, 0, 0,  Texture2D::RGBA16F, data.data());
}

