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

namespace gfx {
class RenderProgram;
class Texture2D;
class VertexArray;
class Buffer;
}

namespace world {
struct Chunk;
}

namespace render {
class WorldChunk: public Drawable {
    public:
        WorldChunk();

        virtual void draw(std::shared_ptr<gfx::RenderProgram> program);
        virtual void frameBegin();

        void setChunk(std::shared_ptr<world::Chunk> chunk);

    private:
        void fillInstanceBuf();

        void updateExposureMap();
        void generateBlockIdMap();

    private:
        // lock around this vector
        std::mutex outstandingWorkLock;
        // work pending by this chunk
        std::vector<std::future<void>> outstandingWork;

        // when set, a full re-calculation of all data is done
        std::atomic_bool withoutCaching = false;

        // chunk to be displayed
        std::shared_ptr<world::Chunk> chunk = nullptr;
        // when set, the chunk instance buffer must be updated for some reason or another
        std::atomic_bool chunkDirty = true;

        // bitmap of exposed blocks. access in 0xYYZZXX coords (TODO: more efficient storage?)
        std::bitset<(256*256*256)> exposureMap;
        /**
         * This vector is identical to the 8-bit ID maps stored in the chunk, except instead of
         * mapping to block UUIDs, they instead map to a bool indicating whether the block is
         * considered air-like for purposes of exposure mapping.
         */
        std::vector<std::array<bool, 256>> exposureIdMaps;

        // when set, the instance buffer must be reloaded
        std::atomic_bool instanceBufDirty;
        // number of instanced values to render
        size_t numInstances = 0;
        // instance data buffer
        std::shared_ptr<gfx::Buffer> instanceBuf = nullptr;

        // vertex array and buffer for a single cube
        std::shared_ptr<gfx::VertexArray> vao = nullptr;
        std::shared_ptr<gfx::Buffer> vbo = nullptr;

        // empty placeholder texture
        std::shared_ptr<gfx::Texture2D> placeholderTex = nullptr;
};
}

#endif
