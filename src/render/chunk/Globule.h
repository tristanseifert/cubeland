/**
 * Globules are small 64x64x64 units of blocks that are the smallest renderable component of a
 * chunk.
 */
#ifndef RENDER_CHUNK_GLOBULE_H
#define RENDER_CHUNK_GLOBULE_H

#include <memory>
#include <atomic>
#include <vector>
#include <bitset>

#include <glbinding/gl/gl.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
}

namespace render::chunk {
class WorldChunkDebugger;

class Globule {
    friend class WorldChunkDebugger;

    public:
        Globule(WorldChunk *chunk, const glm::vec3 pos);
        ~Globule();

        void chunkChanged(const bool isDifferentChunk);

        void startOfFrame();
        void draw(std::shared_ptr<gfx::RenderProgram> &program);

    private:
        struct BlockVertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 uv;
        };

        /*
         * Data passed around when calculating the exposure map, as well as the block contents. It
         * contains mostly a map of which blocks are "air" at, immediately above, and below the
         * current Y level.
         */
        struct AirMap {
            std::bitset<256*256> above, current, below;
        };

    private:
        void transferBuffers();

        void fillBuffer();
        void generateBlockIdMap();
        void insertBlockVertices(const AirMap &am, size_t x, size_t y, size_t z);
        void buildAirMap(std::shared_ptr<world::ChunkSlice> slice, std::bitset<256*256> &map);

    private:
        // position of the globule, in block coordinates, relative to the chunk origin
        glm::vec3 position;
        // chunk we draw data from
        WorldChunk *chunk = nullptr;

        // vertex array used for rendering the block vertex data
        std::shared_ptr<gfx::VertexArray> facesVao = nullptr;

        // kicks off a globule buffer update when set at the beginning of the frame
        std::atomic_bool vertexDataNeedsUpdate = false;
        // when set, the chunk changed and all buffers must be invalidated
        std::atomic_bool invalidateCaches = false;

        // when set, the vertex buffer must be reloaded
        std::atomic_bool vertexBufDirty = false;
        // block vertex buffer
        std::shared_ptr<gfx::Buffer> vertexBuf = nullptr;
        // this buffer contains all the vertices to display
        std::vector<BlockVertex> vertexData;

        // when set, the vertex index buffer is uploaded to the GPU
        std::atomic_bool indexBufDirty = false;
        // buffer containing vertex index data
        std::shared_ptr<gfx::Buffer> indexBuf = nullptr;
        // index buffer for vertex data (TODO: use 16-bit values when possible)
        std::vector<gl::GLuint> indexData;
        // number of indices to render
        size_t numIndices = 0;

        /**
         * This vector is identical to the 8-bit ID maps stored in the chunk, except instead of
         * mapping to block UUIDs, they instead map to a bool indicating whether the block is
         * considered air-like for purposes of exposure mapping.
         */
        std::vector<std::array<bool, 256>> exposureIdMaps;

        /// when set, we are being destructed/want to bail out of work early
        std::atomic_bool abortWork = false;

        /// inhibits the chunk visibility til the next time the index/vertex buffers are uploaded
        bool inhibitDrawing = false;
        /// visibility override flag
        bool isVisible = true;
};
};

#endif
