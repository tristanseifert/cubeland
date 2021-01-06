#include "Renderer.h"
#include "System.h"

#include "util/Frustum.h"
#include "world/block/TextureLoader.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/texture/Texture2D.h"
#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"

#include "io/Format.h"
#include <Logging.h>

#include <mutils/time/profiler.h>
#include <glm/ext.hpp>
#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <glm/gtc/matrix_access.hpp>
#include <imgui.h>
#include <metricsgui/metrics_gui.h>

#include <cstring>

using namespace particles;

/**
 * Vertex data for a 1x1 unit quad.
 */
const gl::GLfloat Renderer::kQuadData[] = {
    // X Y Z coord              UV coord
    -0.5f,  0.5f, 0.0f,         0.0f, 1.0f,
    -0.5f, -0.5f, 0.0f,         0.0f, 0.0f,
     0.5f,  0.5f, 0.0f,         1.0f, 1.0f,
     0.5f, -0.5f, 0.0f,         1.0f, 0.0f,
};

/**
 * Initializes the particle system renderer.
 */
Renderer::Renderer() : RenderStep("Physics", "Particle Renderer") {
    using namespace gfx;

    // create the vertex buffer for particle quads and per particle info
    this->quadVtxBuf = new Buffer(Buffer::Array, Buffer::StaticDraw);
    this->quadVtxBuf->bind();
    this->quadVtxBuf->bufferData(sizeof(kQuadData), (void *) &kQuadData);

    this->particleInfoBuf = new Buffer(Buffer::Array, Buffer::StreamDraw);

    // build vertex array
    this->particleVao = new VertexArray;
    this->particleVao->bind();

    this->quadVtxBuf->bind();
    this->particleVao->registerVertexAttribPointer(0, 3, VertexArray::Float, 5 * sizeof(gl::GLfloat), 0);
    this->particleVao->registerVertexAttribPointer(1, 2, VertexArray::Float, 5 * sizeof(gl::GLfloat), 
            3 * sizeof(gl::GLfloat));

    const size_t kInfoSize = sizeof(ParticleInfo);
    this->particleInfoBuf->bind();
    this->particleVao->registerVertexAttribPointer(2, 3, VertexArray::Float, kInfoSize,
            offsetof(ParticleInfo, pos), 1); // particle position
    this->particleVao->registerVertexAttribPointer(3, 4, VertexArray::Float, kInfoSize,
            offsetof(ParticleInfo, uv), 1); // particle UV
    this->particleVao->registerVertexAttribPointer(4, 1, VertexArray::Float, kInfoSize,
            offsetof(ParticleInfo, scale), 1); // particle scale
    this->particleVao->registerVertexAttribPointer(5, 1, VertexArray::Float, kInfoSize,
            offsetof(ParticleInfo, alpha), 1); // alpha component
    this->particleVao->registerVertexAttribPointer(6, 3, VertexArray::Float, kInfoSize,
            offsetof(ParticleInfo, color), 1); // particle tint

    VertexArray::unbind();

    // read particle texture atlas
    this->particleAtlas = new Texture2D(0);
    // this->particleAtlas->setUsesLinearFiltering(true);
    this->particleAtlas->setDebugName("ParticleAtlas");

    // load shader for drawing particles
    this->shader = new ShaderProgram("misc/particle.vert", "misc/particle.frag");
    this->shader->link();

    this->shader->bind();
    this->shader->setUniform1i("particleTex", this->particleAtlas->unit);

    // create metrics containers
    this->mNumParticles = new MetricsGuiMetric("Active", "particles", MetricsGuiMetric::USE_SI_UNIT_PREFIX);
    this->mVisibleSystems = new MetricsGuiMetric("Visible", "systems", MetricsGuiMetric::USE_SI_UNIT_PREFIX);

    this->mPlot = new MetricsGuiPlot;
    this->mPlot->mInlinePlotRowCount = 3;
    this->mPlot->mShowInlineGraphs = true;
    this->mPlot->mShowAverage = true;
    this->mPlot->mShowLegendUnits = false;

    this->mPlot->AddMetric(this->mNumParticles);
    this->mPlot->AddMetric(this->mVisibleSystems);
}

/**
 * Cleans up renderer resources.
 */
Renderer::~Renderer() {
    delete this->quadVtxBuf;
    delete this->particleInfoBuf;
    delete this->particleVao;
    delete this->particleAtlas;
    delete this->shader;

    delete this->mPlot;
    delete this->mNumParticles;
    delete this->mVisibleSystems;
}



/**
 * Adds a particle system to the renderer.
 */
void Renderer::addSystem(std::shared_ptr<System> &system) {
    LOCK_GUARD(this->particleSystemsLock, ParticleSystems);

    system->setPhysicsEngine(this->phys);
    system->registerTextures(this);

    this->particleSystems.push_back(system);
}

/**
 * Removes an existing particle system.
 */
void Renderer::removeSystem(std::shared_ptr<System> &system) {
    LOCK_GUARD(this->particleSystemsLock, ParticleSystems);

    this->particleSystems.erase(std::remove(this->particleSystems.begin(), 
                this->particleSystems.end(), system), this->particleSystems.end());
}



/**
 * This first culls particle systems whose bounding box is not visible with the current projection
 * matrix. Those particle systems that remain are processed to handle aging of particles (e.g. to
 * create new particles, and destroy old ones) and copy the positions of newly simulated particles
 * out into info buffers.
 */
void Renderer::startOfFrame() {
    LOCK_GUARD(this->particleSystemsLock, ParticleSystems);
    PROFILE_SCOPE(Particles);

    // get the positions of particles from visible particle systems
    const auto projView = this->projectionMatrix * this->viewMatrix;
    util::Frustum frust(projView);

    this->particleInfo.clear();
    size_t numVisibleSystems = 0;

    for(auto &system : this->particleSystems) {
        // check if in view
        glm::vec3 lb, rt;
        system->getBounds(lb, rt);
        if(!frust.isBoxVisible(lb, rt)) {
            // if not, run a particle step but don't spawn new particles.
            system->agingStep(false);
            continue;
        }

        // otherwise, run an aging step that allows spawning, and generate particle info data
        numVisibleSystems++;

        system->agingStep(true);
        system->buildParticleBuf(this->particleInfo);

        // set flags
        this->particleInfoDirty = true;
    }

    this->hasVisibleSystems = (numVisibleSystems > 0);
    this->mVisibleSystems->AddNewValue(numVisibleSystems);

    // particles have to be sorted; draw the furthest particles first
    std::sort(this->particleInfo.begin(), this->particleInfo.end(), [&](const auto &a, const auto &b) {
        const auto distA = distance2(this->viewPosition, a.pos);
        const auto distB = distance2(this->viewPosition, b.pos);

        // return true if distance A < B
        return (distA < distB);
    });

    // atlas updates
    if(this->needsAtlasUpdate) {
        this->rebuildAtlas();

        // do not force a sync to other processors on write for clearing the flag
        this->needsAtlasUpdate.store(false, std::memory_order_relaxed);

        // then run all systems' change callbacks
        for(auto &system : this->particleSystems) {
            system->textureAtlasUpdated(this);
        }
    }

    // also, render debugging window if needed
    if(this->showDebugWindow) {
        this->drawDebugWindow();
    }
}

/**
 * Culls particle systems to only those that are potentially visible.
 */
void Renderer::preRender(render::WorldRenderer *) {
    using namespace gl;

    // disable writing to the depth buffer
    glDepthMask(GL_FALSE);

    // blending additively (to get a nice glow)
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // update the particle info buffer if it became dirtied
    if(this->particleInfoDirty) {
        LOCK_GUARD(this->particleInfoLock, ParticleInfoLock);
        PROFILE_SCOPE(XferParticleBuf);

        const size_t nParticles = this->particleInfo.size();
        this->particleInfoBuf->replaceData(sizeof(ParticleInfo) * nParticles,
                this->particleInfo.data());

        this->numParticles = nParticles;
        this->particleInfoDirty = false;
    }

    this->mNumParticles->AddNewValue(this->numParticles);
}

/**
 * Renders particles from all currently active (and potentially visible) particle systems.
 */
void Renderer::render(render::WorldRenderer *) {
    using namespace gl;
    PROFILE_SCOPE(Particles);

    if(!this->hasVisibleSystems) return;

    // prepare the shader
    this->shader->bind();

    const auto projView = this->projectionMatrix * this->viewMatrix;
    this->shader->setUniformMatrix("projectionView", projView);

    // send the camera right and up vectors; used for billboarding particles
    const auto camRightWs = glm::vec3(row(this->viewMatrix, 0));
    this->shader->setUniformVec("cameraRightWs", camRightWs);
    const auto camUpWs = glm::vec3(row(this->viewMatrix, 1));
    this->shader->setUniformVec("cameraUpWs", camUpWs);

    // draw
    this->particleAtlas->bind();
    this->particleVao->bind();

    if(this->numParticles) {
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, this->numParticles);
    }

    gfx::VertexArray::unbind();
}

/**
 * Restores OpenGL state we changed.
 */
void Renderer::postRender(render::WorldRenderer *) {
    using namespace gl;

    // restore depth writing
    glDepthMask(GL_TRUE);
}



/**
 * Loads a new texture.
 *
 * @return Whether a new texture was allocated or not.
 */
bool Renderer::addTexture(const glm::ivec2 &size, const std::string &path) {
    // build texture info
    TextureInfo info;
    info.size = size;
    info.path = path;

    // add the texture info if needed, or exit if already registered
    {
        LOCK_GUARD(this->texturesLock, ParticleTextures);
        if(this->textures.contains(path)) {
            return false;
        }

        this->textures[path] = info;
    }

    // rebuild the atlas on the next main loop iteration (so it can be directly uploaded)
    this->needsAtlasUpdate = true;

    return true;
}



/**
 * Rebuilds the particle system texture atlas.
 */
void Renderer::rebuildAtlas() {
    // get lock to the textures map
    LOCK_GUARD(this->texturesLock, ParticleTextures);
    PROFILE_SCOPE(RebuildParticleAtlas);

    // repack that hoe
    std::unordered_map<std::string, glm::ivec2> sizes;
    sizes.reserve(this->textures.size());
    for(const auto &[path, info] : this->textures) {
        sizes[path] = info.size;
    }

    this->texturesPacker.updateLayout(sizes);

    // set up the texture data buffer
    const auto atlasSize = this->texturesPacker.getAtlasSize();
    XASSERT(atlasSize.x && atlasSize.y, "Invalid atlas size {}", atlasSize);

    std::vector<std::byte> out;

    const size_t bytesPerPixel = 4 /* components */ * sizeof(float) /*bytes per*/;
    const size_t bytesPerRow = bytesPerPixel * atlasSize.x;

    const size_t numBytes = (atlasSize.x * atlasSize.y) * bytesPerPixel;

    out.resize(numBytes);

    // for each output texture, place it
    std::vector<float> textureBuffer;
    for(const auto &[textureId, bounds] : this->texturesPacker.getLayout()) {
        // get texture and yeet it into the buffer
        const auto &texture = this->textures[textureId];

        textureBuffer.clear();
        textureBuffer.resize(texture.size.x * texture.size.y * 4, 0);

        world::TextureLoader::load(texture.path, textureBuffer);

        // write pointer to the top left of the output
        const auto bytesPerTextureRow = texture.size.x * sizeof(float) * 4;
        std::byte *writePtr = out.data() + (bytesPerRow * bounds.y) + (bytesPerPixel * bounds.x);

        for(size_t y = 0; y < texture.size.y; y++) {
            // calculate offset into texture buffer and yeet it up
            const size_t textureYOff = y * texture.size.x * 4;
            memcpy(writePtr, textureBuffer.data() + textureYOff, bytesPerTextureRow);

            // advance write pointer
            writePtr += bytesPerRow;
        }
    }

    // upload texture
    this->particleAtlas->allocateBlank(atlasSize.x, atlasSize.y, gfx::Texture2D::RGBA16F);
    this->particleAtlas->bufferSubData(atlasSize.x, atlasSize.y, 0, 0,  gfx::Texture2D::RGBA16F,
            out.data());
}

/**
 * Draws the particle system debugger window.
 */
void Renderer::drawDebugWindow() {
    if(!ImGui::Begin("Particle Renderer", &this->showDebugWindow)) {
        return;
    }

    // metrics
    this->mPlot->UpdateAxes();

    if(ImGui::CollapsingHeader("Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
        this->mPlot->DrawList();
    }

    ImGui::End();
}
