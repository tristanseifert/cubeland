#include "WorldChunk.h"
#include "ChunkWorker.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"

#include <Logging.h>
#include <mutils/time/profiler.h>

#include <uuid.h>
#include <glbinding/gl/gl.h>

using namespace render;
using namespace render::chunk;

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

    // create buffers and prepare to bind the vertex attrib object
    this->vao = std::make_shared<VertexArray>();
    this->vbo = std::make_shared<Buffer>(Buffer::Array, Buffer::StaticDraw);

    this->instanceBuf = std::make_shared<Buffer>(Buffer::Array, Buffer::DynamicDraw);

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

    // describe the indexed parameters
    this->instanceBuf->bind();

    // unbind the VAO
    VertexArray::unbind();

    // lastly, load the placeholder texture
    this->placeholderTex = std::make_shared<Texture2D>();
    this->placeholderTex->loadFromImage("/test/empty/whitegreen.png");
}

/**
 * At the start of the frame, fill the instance buffer if needed.
 */
void WorldChunk::frameBegin() {
    if(this->instanceBufDirty) {
        // TODO: transfer instance buffer
        this->instanceBufDirty = false;
    }
}

/**
 * Uses instanced rendering to draw the blocks of the chunk.
 *
 * At this point, our draw list should have been culled to the point that only blocks exposed to
 * air (e.g. ones that could be visible) are in it.
 */
void WorldChunk::draw(std::shared_ptr<gfx::RenderProgram> program) {
    // check for outstanding work
    {
        bool completed = true;
        LOCK_GUARD(this->outstandingWorkLock, OutstandingWork);

        for(const auto &future : this->outstandingWork) {
            if(!future.valid()) {
                completed = false;
            }
        }

        // remove all completed futures
        this->outstandingWork.erase(std::remove_if(this->outstandingWork.begin(),
                    this->outstandingWork.end(), [](std::future<void> &future) {
            return future.valid();
        }), this->outstandingWork.end());

        if(!completed) {
            Logging::warn("Outstanding work at draw call for chunk {}", (void *) this);
        }
    }

    // bind our textures
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



/**
 * Sets the chunk that we're going to be rendering.
 *
 * This immediately kicks off (on the shared chunk worker thread pool) the buffer update
 * computations, since those can take a while. If we're still waiting on this when the draw call
 * comes around, we'll skip updating the buffer and possibly draw stale data.
 */
void WorldChunk::setChunk(std::shared_ptr<world::Chunk> chunk) {
    this->chunk = chunk;
    this->chunkDirty = true;
    this->withoutCaching = true;

    std::future<void> prom = ChunkWorker::pushWork([&]() -> void {
        this->fillInstanceBuf();
    });

    {
        LOCK_GUARD(this->outstandingWorkLock, OutstandingWork);
        this->outstandingWork.push_back(std::move(prom));
    }
}

/**
 * Fills the instance buffer with info on each of the blocks to be drawn.
 *
 * If needed, the "exposed blocks" map is updated as well.
 */
void WorldChunk::fillInstanceBuf() {
    PROFILE_SCOPE(FillInstanceBuf);

    // clear caches if needed
    if(this->withoutCaching) {
        PROFILE_SCOPE(ClearCaches);

        this->exposureIdMaps.clear();
        this->exposureMap.reset();
    }

    // update the exposure ID maps
    if(this->withoutCaching || this->exposureIdMaps.size() != this->chunk->sliceIdMaps.size()) {
        this->generateBlockIdMap();
    }

    // update exposed blocks map if chunk is dirty
    if(this->withoutCaching || this->chunkDirty) {
        this->updateExposureMap();
    }

    // update the actual instance buffer itself
    // TODO: implement this i guess lol

    // ensure the buffer is transferred on the next frame
    this->chunkDirty = false;
    this->instanceBufDirty = true;
}

/**
 * Updates the map of what blocks are exposed.
 */
void WorldChunk::updateExposureMap() {
    PROFILE_SCOPE(UpdateExposureMap);

}

/**
 * Generates the mapping of 8-bit block ids to whether they're air or not
 */
void WorldChunk::generateBlockIdMap() {
    PROFILE_SCOPE(GenerateAirMap);

    // iterate over each input ID map...
    for(const auto &map : this->chunk->sliceIdMaps) {
        PROFILE_SCOPE(ProcessMap);

        // all blocks should be air by default
        std::array<bool, 256> isAir;
        std::fill(isAir.begin(), isAir.end(), true);

        // then, check each of the UUIDs
        for(size_t i = 0; i < map.idMap.size(); i++) {
            const auto &uuid = map.idMap[i];

            // skip if nil UUID
            if(uuid.is_nil()) {
                continue;
            }

            // TODO: check for solidity
            isAir[i] = false;
        }
    }
}
