#include "VertexGeneratorData.h"

#include "gfx/gl/texture/Texture1D.h"
#include "gfx/gl/texture/Texture2D.h"

#include <glm/glm.hpp>

using namespace render::chunk;

/**
 * Sets up the data textures.
 */
VertexGeneratorData::VertexGeneratorData() {
    using namespace gfx;

    // vertex count texture
    this->vtxCountTex = new Texture1D(0);
    this->vtxCountTex->allocateBlank(kMaxVertexTypes, Texture::RED16F);

    // vertex offsets texture
    this->vtxOffsetTex = new Texture2D(1);
}

/**
 * Releases all data textures.
 */
VertexGeneratorData::~VertexGeneratorData() {
    delete this->vtxCountTex;
    delete this->vtxOffsetTex;
}
