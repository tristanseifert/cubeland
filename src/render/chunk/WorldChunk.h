/**
 * Responsible for drawing a single chunk (a pile of blocks) of the world.
 */
#ifndef RENDER_SCENE_WORLDCHUNK_H
#define RENDER_SCENE_WORLDCHUNK_H

#include "render/scene/Drawable.h"

#include <memory>
#include <atomic>
#include <future>
#include <vector>
#include <array>
#include <bitset>
#include <mutex>
#include <unordered_map>

#include <glbinding/gl/gl.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace gfx {
class RenderProgram;
class Texture2D;
class VertexArray;
class Buffer;
}

namespace world {
struct Chunk;
struct ChunkSlice;
}

namespace render {
namespace chunk {
class WorldChunkDebugger;
}

class WorldChunk: public Drawable {
    friend class chunk::WorldChunkDebugger;

    public:
        WorldChunk();

        virtual void draw(std::shared_ptr<gfx::RenderProgram> program);
        void drawHighlights(std::shared_ptr<gfx::RenderProgram> program);

        virtual void frameBegin();

        void setChunk(std::shared_ptr<world::Chunk> chunk);

        /// render program (for forward rendering)
        static std::shared_ptr<gfx::RenderProgram> getProgram();
        /// render program for drawing the highlights
        static std::shared_ptr<gfx::RenderProgram> getHighlightProgram();
        /// render program for shadow rendering
        static std::shared_ptr<gfx::RenderProgram> getShadowProgram();

    public:
        uint64_t addHighlight(const glm::vec3 &start, const glm::vec3 &end);
        bool removeHighlight(const uint64_t id);

        // whether this chunk needs to participate in the outline drawing process
        bool needsDrawHighlights() const {
            return this->hasHighlights;
        }

    private:
        struct BlockVertex {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 uv;
        };

        struct HighlightInfo {
            // extents of the highlighting zone
            glm::vec3 start, end;
            // when set, a stroke outline is used
            bool outline = true;
            // when set, the outline is drawn as a wireframe
            bool wireframe = false;

            // color of the highlight
            glm::vec3 color = glm::vec3(0, 1, 0);
        };

        struct HighlightInstanceData {
            // transform matrix (both regular and scaled)
            glm::mat4 transform, scaled;
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
        void initHighlightBuffer();
        void updateHighlightBuffer();

        void transferBuffers();

        void fillInstanceBuf();
        void insertBlockVertices(const AirMap &am, size_t x, size_t y, size_t z);

        void updateExposureMap();
        void generateBlockIdMap();
        void buildAirMap(std::shared_ptr<world::ChunkSlice> slice, std::bitset<256*256> &map);

    private:
        // lock for the highlights array
        std::mutex highlightsLock;
        // cached flag indicating whether there are any highlights
        std::atomic_bool hasHighlights;
        // regions of blocks to highlight
        std::unordered_map<uint64_t, HighlightInfo> highlights;
        // next id for highlights
        std::atomic_uint_least64_t highlightsId = 1;
        // when set, the highlight buffers are regenerated in the background
        std::atomic_bool highlightsNeedUpdate;
        // indicates the highlight buffers have updated and need to be uploaded to the GPU
        std::atomic_bool highlightsBufDirty;

        // CPU buffer of highlight data
        std::vector<HighlightInstanceData> highlightData;
        // GPU buffer of highlight data
        std::shared_ptr<gfx::Buffer> highlightBuf = nullptr;
        // vertex array for highlight data. it takes the same vertex data but uses a transform matrix
        std::shared_ptr<gfx::VertexArray> highlightVao = nullptr;
        // number of highlights to draw
        size_t numHighlights = 0;

        // when set, a full re-calculation of all data is done
        std::atomic_bool withoutCaching = false;

        // chunk to be displayed
        std::shared_ptr<world::Chunk> chunk = nullptr;
        // when set, the chunk instance buffer must be updated for some reason or another
        std::atomic_bool chunkDirty = true;

        // force exposure map update if set
        std::atomic_bool exposureMapNeedsUpdate = false;
        // bitmap of exposed blocks. access in 0xYYZZXX coords (TODO: more efficient storage?)
        //std::bitset<(256*256*256)> exposureMap;
        std::vector<bool> exposureMap;
        /**
         * This vector is identical to the 8-bit ID maps stored in the chunk, except instead of
         * mapping to block UUIDs, they instead map to a bool indicating whether the block is
         * considered air-like for purposes of exposure mapping.
         */
        std::vector<std::array<bool, 256>> exposureIdMaps;

        // when set, the instance buffer is updated at the start of the next frame
        std::atomic_bool instanceDataNeedsUpdate = false;

        // vertex array used for rendering the block vertex data
        std::shared_ptr<gfx::VertexArray> facesVao = nullptr;

        // when set, the vertex buffer must be reloaded
        std::atomic_bool vertexBufDirty = false;
        // block vertex buffer
        std::shared_ptr<gfx::Buffer> vertexBuf = nullptr;
        // this buffer contains all the vertices to display
        std::vector<BlockVertex> vertexData;

        // when set, the vertex index buffer is uploaded to the GPU
        std::atomic_bool indexBufDirty = false;
        // buffer containing vertex index data
        std::shared_ptr<gfx::Buffer> vertexIndexBuf = nullptr;
        // index buffer for vertex data
        std::vector<gl::GLuint> vertexIndexData;
        // number of indices to render
        size_t numIndices = 0;

        // vertex array and buffer for a single cube
        std::shared_ptr<gfx::VertexArray> placeholderVao = nullptr;
        std::shared_ptr<gfx::Buffer> vbo = nullptr;

        // empty placeholder texture
        std::shared_ptr<gfx::Texture2D> placeholderTex = nullptr;

        // debugger for this world chunk
        std::shared_ptr<chunk::WorldChunkDebugger> debugger = nullptr;
        // when set, all chunks are drown in wireframe mode rather than solid
        bool drawWireframe = false;
};
}

#endif
