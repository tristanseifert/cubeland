#include "WorldChunk.h"
#include "WorldChunkDebugger.h"
#include "Globule.h"
#include "ChunkWorker.h"
#include "VertexGenerator.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/model/RenderProgram.h"

#include "world/chunk/Chunk.h"
#include "world/chunk/ChunkSlice.h"
#include "world/block/BlockRegistry.h"
#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <uuid.h>
#include <glbinding/gl/gl.h>
#include <glm/ext.hpp>

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

/// render program (for forward rendering)
std::shared_ptr<gfx::RenderProgram> WorldChunk::getProgram() {
    auto p = std::make_shared<gfx::RenderProgram>("model/chunk.vert", "model/chunk.frag", true);
    p->link();
    return p;
}
/// render program for highlight rendering
std::shared_ptr<gfx::RenderProgram> WorldChunk::getHighlightProgram() {
    auto p = std::make_shared<gfx::RenderProgram>("model/chunk_highlight.vert",
            "model/chunk_highlight.frag", true);
    p->link();
    return p;
}
/// render program for shadow rendering
std::shared_ptr<gfx::RenderProgram> WorldChunk::getShadowProgram() {
    auto p = std::make_shared<gfx::RenderProgram>("model/chunk_shadow.vert",
            "model/chunk_shadow.frag", false);
    p->link();
    return p;
}

/**
 * Sets up the static buffers used to draw the blocks in the world.
 */
WorldChunk::WorldChunk() {
    using namespace gfx;

    // set up the placeholder vertex array
    this->vbo = new Buffer(Buffer::Array, Buffer::StaticDraw);
    this->vbo->bind();
    this->vbo->bufferData(sizeof(kCubeVertices), (void *) &kCubeVertices);
    this->vbo->unbind();

    this->placeholderVao = new VertexArray;

    this->vbo->bind();
    this->placeholderVao->registerVertexAttribPointer(0, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat), 
            0); // vertex position
    this->placeholderVao->registerVertexAttribPointer(1, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat)); // normals
    this->placeholderVao->registerVertexAttribPointer(2, 2, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            6 * sizeof(gl::GLfloat)); // texture coordinate

    VertexArray::unbind();

    // set up highlighting and allocate some other buffers
    this->initHighlightBuffer();

    // allocate globules
    for(size_t y = 0; y < 256/kGlobuleSize; y++) {
        for(size_t z = 0; z < 256/kGlobuleSize; z++) {
            for(size_t x = 0; x < 256/kGlobuleSize; x++) {
                const glm::ivec3 pos(x * kGlobuleSize, y * kGlobuleSize, z * kGlobuleSize);

                auto glob = new Globule(this, pos);
                this->globules[pos] = glob;
            }
        }
    }
}

/**
 * Deletes the globules.
 */
WorldChunk::~WorldChunk() {
    if(this->vtxGenCallbackToken) {
        VertexGenerator::unregisterCallback(this->vtxGenCallbackToken);
        this->vtxGenCallbackToken = 0;
    }

    for(auto [key, globule] : this->globules) {
        delete globule;
    }

    if(this->debugger) {
        delete this->debugger;
    }

    delete this->highlightBuf;
    delete this->highlightVao;
    delete this->placeholderVao;
    delete this->vbo;
}

/**
 * Perform highlight calculations right here; these don't have a lot of data to go over so the
 * overhead of pushing them to the background is not really worth it.
 *
 * Also invoke all globules, which may kick off background buffer filling.
 */
void WorldChunk::frameBegin() {
    // highlight related stuff
    if(this->highlightsNeedUpdate) { // queue updating of buffer in background
        this->highlightsNeedUpdate = false;
        this->updateHighlightBuffer();
    }

    // draw debugger
    if(this->debugger) {
        this->debugger->draw();
    }
}

/**
 * Uses instanced rendering to draw the blocks of the chunk.
 *
 * At this point, our draw list should have been culled to the point that only blocks exposed to
 * air (e.g. ones that could be visible) are in it.
 */
void WorldChunk::draw(std::shared_ptr<gfx::RenderProgram> &program, const bool special) {
    PROFILE_SCOPE(ChunkDraw);

    using namespace gl;

    // set up for rendering
    program->bind();

    // a chunk was loaded; try to draw each globule
    if(this->chunk) {
        if(this->drawWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        for(const auto &[key, globule] : this->globules) {
            if(special) {
                globule->drawSpecial(program);
            } else {
                globule->draw(program);
            }
        }

        if(this->drawWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    // if no chunk data available, draw a wireframe outline of the chunk
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        this->placeholderVao->bind();
        glDrawArrays(GL_TRIANGLES, 0, 36);
        gfx::VertexArray::unbind();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}



/**
 * Sets the chunk that we're going to be rendering.
 *
 * This immediately kicks off (on the shared chunk worker thread pool) the buffer update
 * computations, since those can take a while. If we're still waiting on this when the draw call
 * comes around, we'll skip updating the buffer and possibly draw stale data.
 */
void WorldChunk::setChunk(std::shared_ptr<world::Chunk> chunk) {
    using namespace std::placeholders;

    // remove any old chunk and vertex generator observers
    if(this->chunkChangeToken) {
        if(this->chunk) {
            this->chunk->unregisterChangeCallback(this->chunkChangeToken);
        }
        this->chunkChangeToken = 0;
    }
    if(this->vtxGenCallbackToken) {
        VertexGenerator::unregisterCallback(this->vtxGenCallbackToken);
        this->vtxGenCallbackToken = 0;
    }

    // clear globule buffers if chunk is nullptr
    this->chunk = chunk;
    if(!chunk) {
        for(auto &[key, globule] : this->globules) {
            globule->clearBuffers();
        }

        return;
    }

    // install vertex generator callback
    this->vtxGenCallbackToken = VertexGenerator::registerCallback(chunk->worldPos,
            std::bind(&WorldChunk::vtxGenCallback, this, _1, _2));

    // request generation of vertices for ALL globules in the chunk
    VertexGenerator::update(chunk);

    // install new observer
    this->chunkChangeToken = chunk->registerChangeCallback(
            std::bind(&WorldChunk::blockDidChange, this, _1, _2, _3, _4));
}

/**
 * Called when a globule's buffer has been regenerated by the vertex generator.
 *
 * @note This may not be run on the main thread.
 */
void WorldChunk::vtxGenCallback(const glm::ivec2 &chunkPos, const chunk::VertexGenerator::BufList &buffers) {
    // ensure we've still got the same chunk
    if(!this->chunk || this->chunk->worldPos != chunkPos) {
        if(this->chunk) {
            Logging::error("Received vertex generator callback for chunk {}, but have {}", chunkPos, this->chunk->worldPos);
        }
        return;
    }

    // for each of the buffers, assign it to the corresponding globule
    for(const auto &[globulePos, bufInfo] : buffers) {
        this->globules[globulePos]->setBuffer(bufInfo);
    }
}

/**
 * Notifies us that a block inside the chunk was changed. This is used so that we can automagically
 * update the globule holding that chunk.
 */
void WorldChunk::blockDidChange(world::Chunk *, const glm::ivec3 &blockCoord, const world::Chunk::ChangeHints hints, const uuids::uuid &) {
    // update the block
    this->markBlockChanged(blockCoord);
    // Logging::trace("Block {} changed, flags {}", blockCoord, hints);
}
/**
 * Marks a block as changed. This will cause the globule at the given coordinate to regenerate its
 * internal buffers.
 *
 * TODO: we should coalesce multiple update requests for the same globule.
 *
 * @note `pos` is relative to the origin of the chunk.
 */
void WorldChunk::markBlockChanged(const glm::ivec3 &pos) {
    // we want to update the globule that holds this chunk
    const auto globuleOff = pos / glm::ivec3(kGlobuleSize);
    const auto globuleOrigin = globuleOff * glm::ivec3(kGlobuleSize);

    uint64_t bits = VertexGenerator::blockPosToBits(globuleOrigin);

    // if the X coordinate is on an edge of the globule, update its neighbor
    if(pos.x % 64 == 0) {
        if(globuleOrigin.x >= 64) {
            bits |= VertexGenerator::blockPosToBits(globuleOrigin - glm::ivec3(64, 0, 0));
        } else {
            // TODO: this globule is in an adjacent chunk
        }
    } else if(pos.x % 64 == 63) {
        if(globuleOrigin.x < 192) {
            bits |= VertexGenerator::blockPosToBits(globuleOrigin + glm::ivec3(0, 0, 64));
        } else {
            // TODO: this globule is in an adjacent chunk
        }
    }

    // if the Y coordinate is on the edge of a globule, update its neighbor
    if(pos.y % 64 == 0 && globuleOrigin.y >= 64) {
        bits |= VertexGenerator::blockPosToBits(globuleOrigin - glm::ivec3(0, 64, 0));
    } else if(pos.y % 64 == 63 && globuleOrigin.y < 192) {
        bits |= VertexGenerator::blockPosToBits(globuleOrigin + glm::ivec3(0, 64, 0));
    }

    // if the Z coordinate is on an edge of the globule, update its neighbor
    if(pos.z % 64 == 0) {
        if(globuleOrigin.z >= 64) {
            bits |= VertexGenerator::blockPosToBits(globuleOrigin - glm::ivec3(0, 0, 64));
        } else {
            // TODO: this globule is in an adjacent chunk
        }
    } else if(pos.z % 64 == 63) {
        if(globuleOrigin.z < 192) {
            bits |= VertexGenerator::blockPosToBits(globuleOrigin + glm::ivec3(0, 0, 64));
        } else {
            // TODO: this globule is in an adjacent chunk
        }
    }

    // update all of the globules we requested
    VertexGenerator::update(this->chunk, bits, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Highlighting stuff
/**
 * Adds a new highlighting section.
 */
uint64_t WorldChunk::addHighlight(const glm::vec3 &start, const glm::vec3 &end, const glm::vec4 &color, const glm::mat4 &transform) {
    uint64_t id = this->highlightsId++;

    // create highlights info and submit it
    HighlightInfo info;
    info.start = start;
    info.end = end;
    info.color = color;
    info.transform = transform;

    {
        LOCK_GUARD(this->highlightsLock, AddHighlight);
        this->highlights[id] = info;
    }

    // queue highlight buffer updating at the start of next frame
    this->highlightsNeedUpdate = true;
    this->hasHighlights = true;

    return id;
}

/**
 * Removes a highlight with the given id.
 */
bool WorldChunk::removeHighlight(const uint64_t id) {
    LOCK_GUARD(this->highlightsLock, RemoveHighlight);

    bool success = (this->highlights.erase(id) == 1);
    if(success) {
        this->highlightsNeedUpdate = true;
    }

    this->hasHighlights = !this->highlights.empty();
    return success;
}

/**
 * Sets the color of an existing highlight.
 */
void WorldChunk::setHighlightColor(const uint64_t id, const glm::vec4 &color) {
    this->highlights[id].color = color;
    this->highlightsNeedUpdate = true;
}


/**
 * Initializes highlight buffer and vertex arrays
 */
void WorldChunk::initHighlightBuffer() {
    using namespace gfx;

    // allocate them
    this->highlightVao = new VertexArray;
    this->highlightBuf = new Buffer(Buffer::Array, Buffer::DynamicDraw);

    this->highlightVao->bind();

    // define per-vertex attributes
    this->vbo->bind();

    this->highlightVao->registerVertexAttribPointer(0, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat), 
            0); // vertex position
    this->highlightVao->registerVertexAttribPointer(1, 3, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat)); // normals
    this->highlightVao->registerVertexAttribPointer(2, 2, VertexArray::Float, 8 * sizeof(gl::GLfloat),
            6 * sizeof(gl::GLfloat)); // texture coordinate

    // clean up
    VertexArray::unbind();
}
/**
 * Update the highlight buffers.
 *
 * Each of the highlights is drawn as a cube stretched to fill the extents.
 */
void WorldChunk::updateHighlightBuffer() {
    PROFILE_SCOPE(UpdateHighlightBuf);
    LOCK_GUARD(this->highlightsLock, RemoveHighlight);

    this->highlightData.clear();

    // iterate over all highlights
    for(const auto &[id, info] : this->highlights) {
        HighlightInstanceData data;

        // calculate the scale/translations
        glm::mat4 translation(1), scaled(1);

        float xScale = fabs(info.start.x - info.end.x);
        float yScale = fabs(info.start.y - info.end.y);
        float zScale = fabs(info.start.z - info.end.z);
        glm::vec3 scale(xScale, yScale, zScale);

        // translation = glm::translate(translation, glm::vec3(0.01, 0.01, 0.01));
        translation = glm::translate(translation, info.start);

        translation = glm::translate(translation, glm::vec3(xScale/2, yScale/2, zScale/2));
        scaled = glm::scale(translation, glm::vec3(1.25, 1.25, 1.25));

        // Logging::trace("Scale for extents {},{} -> {}", info.start, info.end, scale);
        translation = glm::scale(translation, scale);
        scaled = glm::scale(scaled, scale);

        data.color = info.color;
        data.transform = translation * info.transform;
        data.scaled = scaled * info.transform;

        // inscrete it
        this->highlightData.push_back(data);
    }

    // mark buffer as to be updated
    // Logging::trace("Highlighting data size: {}", this->highlightData.size());
    this->highlightsBufDirty = true;
}



/**
 * Draws the highlights.
 */
void WorldChunk::drawHighlights(std::shared_ptr<gfx::RenderProgram> &program) {
    using namespace gl;
    PROFILE_SCOPE(DrawHighlights);

    // transfer the highlighting buffer if it's ready, and bail if there's nothing to draw
    if(this->highlightsBufDirty) { // upload buffer
        const auto size = sizeof(HighlightInstanceData) * this->highlightData.size();
        if(size) {
            this->highlightBuf->bind();
            this->highlightBuf->bufferData(size, this->highlightData.data());
            this->highlightBuf->unbind();
        }

        this->numHighlights = this->highlightData.size();
        this->highlightsBufDirty = false;
    }

    if(!this->numHighlights) {
        return;
    }

    // set the highlight color
    program->setUniform1f("WriteColor", 1);

    // draw the highlights, ignoring depth
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    this->highlightVao->bind();
    for(const auto &data: this->highlightData) {
        program->setUniformVec("HighlightColor", data.color);
        program->setUniformMatrix("model2", data.transform);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // clean up
    gfx::VertexArray::unbind();
    glEnable(GL_DEPTH_TEST);
}


void CalculateTangents() {
    const size_t vertices = 36;
    const size_t stride = 8;

    for(size_t i = 0; i < vertices; i += 3) {
        const size_t off = (i * stride);

        // get positions
        const glm::vec3 v0(kCubeVertices[off+0], kCubeVertices[off+1], kCubeVertices[off+2]);
        const glm::vec3 v1(kCubeVertices[off+stride+0], kCubeVertices[off+stride+1],
                kCubeVertices[off+stride+2]);
        const glm::vec3 v2(kCubeVertices[off+stride*2+0], kCubeVertices[off+stride*2+1],
                kCubeVertices[off+stride*2+2]);

        // get UVs
        const glm::vec2 uv0(kCubeVertices[off+6], kCubeVertices[off+7]);
        const glm::vec2 uv1(kCubeVertices[off+stride+6], kCubeVertices[off+stride+7]);
        const glm::vec2 uv2(kCubeVertices[off+stride*2+6], kCubeVertices[off+stride*2+7]);

        // Edges of the triangle : position delta
        glm::vec3 deltaPos1 = v1-v0;
        glm::vec3 deltaPos2 = v2-v0;

        // UV delta
        glm::vec2 deltaUV1 = uv1-uv0;
        glm::vec2 deltaUV2 = uv2-uv0;

        // i want to fucking die
        float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
        glm::vec3 tangent = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y)*r;
        glm::vec3 bitangent = (deltaPos2 * deltaUV1.x   - deltaPos1 * deltaUV2.x)*r;

        Logging::trace("{} -> tangent {} bitangent {}", i, tangent, bitangent);
    }
}
