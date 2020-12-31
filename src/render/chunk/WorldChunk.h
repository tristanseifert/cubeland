/**
 * Responsible for drawing a single chunk (a pile of blocks) of the world.
 *
 * Each 256x256x256 chunk is in turn broken down into smaller 64x64x64 globules, which are the
 * smallest unit that we render. Each globule maintains its own vertex/index buffers, and can
 * automagically update them as needed.
 *
 * To draw each globule, it generates internally a mesh for its blocks. This consists of all of
 * the exposed faces of the block.
 */
#ifndef RENDER_SCENE_WORLDCHUNK_H
#define RENDER_SCENE_WORLDCHUNK_H

#include "world/chunk/Chunk.h"
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
#include <glm/gtx/hash.hpp>

namespace gfx {
class RenderProgram;
class Texture2D;
class VertexArray;
class Buffer;
}

namespace world {
struct ChunkSlice;
}

namespace physics {
class Engine;
}

namespace render {
namespace chunk {
class WorldChunkDebugger;
class Globule;
}
namespace scene {
class ChunkLoader;
}

class WorldChunk: public Drawable {
    friend class chunk::WorldChunkDebugger;
    friend class chunk::Globule;
    friend class scene::ChunkLoader;

    public:
        WorldChunk(physics::Engine *physics);
        virtual ~WorldChunk();

        virtual void draw(std::shared_ptr<gfx::RenderProgram> &program);
        void drawHighlights(std::shared_ptr<gfx::RenderProgram> &program);

        virtual void frameBegin();

        void setChunk(std::shared_ptr<world::Chunk> chunk);

        /// render program (for forward rendering)
        static std::shared_ptr<gfx::RenderProgram> getProgram();
        /// render program for drawing the highlights
        static std::shared_ptr<gfx::RenderProgram> getHighlightProgram();
        /// render program for shadow rendering
        static std::shared_ptr<gfx::RenderProgram> getShadowProgram();

        void markBlockChanged(const glm::ivec3 &pos);

        uint64_t addHighlight(const glm::vec3 &start, const glm::vec3 &end, const glm::vec4 &color = glm::vec4(0, 1, 0, .74));
        void setHighlightColor(const uint64_t id, const glm::vec4 &color);
        bool removeHighlight(const uint64_t id);

        // whether this chunk needs to participate in the outline drawing process
        bool needsDrawHighlights() const {
            return this->hasHighlights;
        }

    private:
        void blockDidChange(const glm::ivec3 &blockCoord, const world::Chunk::ChangeHints hints);

    private:
        /// size of a globule, cubed
        constexpr static const size_t kGlobuleSize = 64;

        struct HighlightInfo {
            // extents of the highlighting zone
            glm::vec3 start, end;
            // when set, a stroke outline is used
            bool outline = true;
            // when set, the outline is drawn as a wireframe
            bool wireframe = false;

            // color of the highlight
            glm::vec4 color = glm::vec4(0, 1, 0, .74);
        };

        struct HighlightInstanceData {
            // color to use for this highlight
            glm::vec4 color;
            // transform matrix (both regular and scaled)
            glm::mat4 transform, scaled;
        };

    private:
        void initHighlightBuffer();
        void updateHighlightBuffer();

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

        // chunk to be displayed
        std::shared_ptr<world::Chunk> chunk = nullptr;
        // the globules that make up the chunk (for rendering)
        std::unordered_map<glm::ivec3, chunk::Globule *> globules;

        // id of the chunk change handler (or 0)
        size_t chunkChangeToken = 0;

        // vertex array and buffer for a single cube
        std::shared_ptr<gfx::VertexArray> placeholderVao = nullptr;
        std::shared_ptr<gfx::Buffer> vbo = nullptr;

        // debugger for this world chunk
        std::shared_ptr<chunk::WorldChunkDebugger> debugger = nullptr;
        // when set, all chunks are drown in wireframe mode rather than solid
        bool drawWireframe = false;
};
}

#endif
