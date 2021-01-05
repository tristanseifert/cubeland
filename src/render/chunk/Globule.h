/**
 * Globules are small 64x64x64 units of blocks that are the smallest renderable component of a
 * chunk.
 */
#ifndef RENDER_CHUNK_GLOBULE_H
#define RENDER_CHUNK_GLOBULE_H

#include "VertexGenerator.h"
#include "world/block/Block.h"

#include <memory>
#include <atomic>
#include <vector>
#include <bitset>
#include <array>
#include <future>
#include <variant>

#include <glbinding/gl/types.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_precision.hpp>

namespace reactphysics3d {
class TriangleVertexArray;
class TriangleMesh;
class ConcaveMeshShape;
class RigidBody;
class Collider;
}


namespace render {
class WorldChunk;
}

namespace world {
struct ChunkSlice;
}

namespace gfx {
class VertexArray;
class Buffer;
class RenderProgram;
class Texture2D;
}

namespace render::chunk {
class WorldChunkDebugger;

class Globule {
    friend class WorldChunkDebugger;

    public:
        Globule(WorldChunk *chunk, const glm::ivec3 pos);
        ~Globule();

        void chunkChanged(const bool isDifferentChunk);
        void clearBuffers();
        void setBuffer(const VertexGenerator::Buffer &buf);

        void startOfFrame();
        void draw(std::shared_ptr<gfx::RenderProgram> &program);

        void eraseBlockAt(const glm::ivec3 &pos);
        void insertBlockAt(const glm::ivec3 &pos, const uuids::uuid &newId);
        void updateBlockAt(const glm::ivec3 &pos, const uuids::uuid &newId);

    public:
        static void fillNormalTex(gfx::Texture2D *tex);

    private:
        int vertexIndexForBlock(const glm::ivec3 &blockOff);

    private:
        // position of the globule, in block coordinates, relative to the chunk origin
        glm::ivec3 position;

        // vertex array used for rendering the block vertex data
        gfx::VertexArray *facesVao = nullptr;

        // block vertex buffer
        std::shared_ptr<gfx::Buffer> vertexBuf = nullptr;
        // number of vertices
        size_t numVertices = 0;
        // buffer containing vertex index data
        std::shared_ptr<gfx::Buffer> indexBuf = nullptr;
        // number of indices to render
        size_t numIndices = 0;
        // index format
        gl::GLenum indexFormat;

        /// inhibits the chunk visibility til the next time the index/vertex buffers are uploaded
        bool inhibitDrawing = false;
        /// visibility override flag
        bool isVisible = true;
};
};

#endif
