/**
 * Handles generating the data textures used by the vertex generator.
 */
#ifndef RENDER_CHUNK_VERTEXGENERATORDATA_H
#define RENDER_CHUNK_VERTEXGENERATORDATA_H

#include <cstddef>

namespace gfx {
class Texture1D;
class Texture2D;
}

namespace render::chunk {
class VertexGeneratorData {
    public:
        VertexGeneratorData();
        ~VertexGeneratorData();

    public:
        /// Maximum number of different block models
        constexpr static const size_t kMaxVertexTypes = 128;
        /// Maximum number of vertices per model
        constexpr static const size_t kMaxVertices = 64;

    private:
        gfx::Texture1D *vtxCountTex = nullptr;
        gfx::Texture2D *vtxOffsetTex = nullptr;
};
}

#endif
