#include "EngineDebugRenderer.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"

// physics library has a few warnings that need to be fixed but we'll just suppress them for now
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#include <reactphysics3d/reactphysics3d.h> 
#pragma GCC diagnostic pop

#include <glbinding/gl/gl.h>
#include <mutils/time/profiler.h>

using namespace physics;

/**
 * Initializes buffers for each of the point and line buffers, as well as the shader used to draw
 * them.
 */
EngineDebugRenderer::EngineDebugRenderer() {
    using namespace gfx;

    // buffers for drawing lines
    this->lineVbo = new Buffer(Buffer::Array, Buffer::DynamicDraw);
    this->lineVao = new VertexArray;

    this->lineVao->bind();
    this->lineVbo->bind();

    const size_t kLineVtxSize = sizeof(reactphysics3d::Vector3) + sizeof(reactphysics3d::uint32);
    this->lineVao->registerVertexAttribPointer(0, 3, VertexArray::Float, kLineVtxSize, 0);
    this->lineVao->registerVertexAttribPointerInt(1, 1, VertexArray::UnsignedInteger, kLineVtxSize,
            sizeof(reactphysics3d::Vector3));

    VertexArray::unbind();

    // buffers for drawing triangles
    this->triangleVbo = new Buffer(Buffer::Array, Buffer::DynamicDraw);
    this->triangleVao = new VertexArray;

    this->triangleVao->bind();
    this->triangleVbo->bind();

    const size_t kTriangleVtxSize = sizeof(reactphysics3d::Vector3) + sizeof(reactphysics3d::uint32);
    this->triangleVao->registerVertexAttribPointer(0, 3, VertexArray::Float, kTriangleVtxSize, 0);
    this->triangleVao->registerVertexAttribPointerInt(1, 1, VertexArray::UnsignedInteger, 
            kTriangleVtxSize, sizeof(reactphysics3d::Vector3));

    VertexArray::unbind();

    // lastly, our rendering shader
    this->shader = new ShaderProgram("misc/physics_debug.vert", "misc/physics_debug.frag");
    this->shader->link();
}

/**
 * Releases all buffers and shaders.
 */
EngineDebugRenderer::~EngineDebugRenderer() {
    delete this->lineVbo;
    delete this->lineVao;
    delete this->triangleVbo;
    delete this->triangleVao;
    delete this->shader;
}



/**
 * Refills the point and line vertex buffers as needed.
 *
 * TODO: we could possibly be smarter about when to refill these buffers!
 */
void EngineDebugRenderer::preRender(render::WorldRenderer *) {
    PROFILE_SCOPE(PhysicsDebugPreRender);

    using DebugLine = reactphysics3d::DebugRenderer::DebugLine;
    using DebugTriangle = reactphysics3d::DebugRenderer::DebugTriangle;

    if(!this->drawDebugData) return;

    auto &dr = this->world->getDebugRenderer();

    // update lines buffer
    if(dr.getNbLines() > 0) {
        const size_t size = dr.getNbLines() * sizeof(DebugLine);
        this->lineVbo->replaceData(size, dr.getLinesArray());
    }

    // update triangles buffer
    if(dr.getNbTriangles() > 0) {
        const size_t size = dr.getNbTriangles() * sizeof(DebugTriangle);
        this->triangleVbo->replaceData(size, dr.getTrianglesArray());
    }
}

/**
 * Draws the points and lines requested by the physics engine. These are drawn as wireframes.
 */
void EngineDebugRenderer::render(render::WorldRenderer *) {
    PROFILE_SCOPE(PhysicsDebug);
    using namespace gl;

    if(!this->drawDebugData) return;

    auto &dr = this->world->getDebugRenderer();

    // prepare shader
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    this->shader->bind();

    glm::mat4 model(1);
    this->shader->setUniformMatrix("model", model);

    const auto projView = this->projectionMatrix * this->viewMatrix;
    this->shader->setUniformMatrix("projectionView", projView);

    // draw lines
    if(dr.getNbLines() > 0) {
        this->lineVao->bind();
        this->lineVbo->bind();

        glDrawArrays(GL_LINES, 0, dr.getNbLines() * 2);

        this->lineVbo->unbind();
    }

    // draw triangles
    if(dr.getNbTriangles() > 0) {
        this->triangleVao->bind();
        this->triangleVbo->bind();

        glDrawArrays(GL_TRIANGLES, 0, dr.getNbTriangles() * 3);

        this->triangleVao->unbind();
    }

    // clean up
    gfx::VertexArray::unbind();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
