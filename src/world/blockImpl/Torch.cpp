#include "Torch.h"

#include "world/chunk/Chunk.h"
#include "world/block/TextureLoader.h"
#include "world/block/BlockRegistry.h"

#include "gfx/lights/PointLight.h"
#include "particles/System.h"

#include "io/Format.h"
#include <Logging.h>

using namespace world::blocks;

/**
 * Torch smoke particle system
 */
class TorchSmoke: public particles::System {
    public:
        TorchSmoke(const glm::vec3 &pos) : System(pos) {
            this->maxParticles = 35;
            this->spawnRounds = 2;
            this->spawnProbability = .05;

            this->deathLength = 42;
            this->minParticleAge = 30;
            this->maxParticleAge = 180;

            this->initialForce = glm::vec3(0, .001, 0);
            this->forceVariation = glm::vec3(.00033, .0005, .00033);
        }

        /**
         * Register the smoke texture; it is 16x16, with 12 frames of animation.
         */
        void registerTextures(particles::Renderer *rend) override {
            if(!rend->addTexture(glm::vec2(16, 192), "particle/bigsmoke.png")) {
                this->textureAtlasUpdated(rend);
            }
        }

        /**
         * Gets the UV of the default texture and saves it as the cached UV. value.
         */
        void textureAtlasUpdated(particles::Renderer *rend) override {
            this->smokeUv = rend->getUv("particle/bigsmoke.png");
        }

        /**
         * Based on the age of the particle, pick the correct of the 12 animation frames, and
         * scale the UV coordinates appropriately.
         */
        glm::vec4 uvForParticle(const particles::System::Particle &particle) override {
            // calculate the UV Y range for a single one of the 12 textures
            const float yRange = this->smokeUv.w - this->smokeUv.y;
            const float frameY = yRange / 12.;

            // select which of the 12 frames we want
            const float life = ((float) particle.age) / ((float) particle.maxAge);
            const int frame = floor(life * 11.);

            // scale the UV
            auto uv = this->smokeUv;
            uv.y += (frame * frameY);
            uv.w = uv.y + frameY;

            return uv;
        }

        /**
         * Smoke particles from torches are darker.
         */
        glm::vec3 tintForParticle(const Particle &particle) override {
            return glm::vec3(.5);
        }

    private:
        /// UV for the entire 16x192 smoke particle texture
        glm::vec4 smokeUv;
};

/**
 * Model for a vertical torch. This is the same as a block, but it's only .15 units in width/depth
 * and .74 units tall.
 */
const world::BlockRegistry::Model Torch::kVerticalModel = {
    .vertices = {
        // top face
        {.40, .74, .60}, {.60, .74, .60}, {.60, .74, .40}, {.40, .74, .40},
        // left face
        {.40,   0, .60}, {.40, .74, .60}, {.40, .74, .40}, {.40,   0, .40},
        // right face
        {.60,   0, .40}, {.60, .74, .40}, {.60, .74, .60}, {.60,   0, .60},
        // front face
        {.40, .74, .40}, {.60, .74, .40}, {.60,   0, .40}, {.40,   0, .40},
        // back face
        {.40,   0, .60}, {.60,   0, .60}, {.60, .74, .60}, {.40, .74, .60},
    },
    .faceVertIds = {
        // top face
        {1, 0}, {1, 1}, {1, 2}, {1, 3},
        // left face
        {2, 0}, {2, 1}, {2, 2}, {2, 3},
        // right face
        {3, 0}, {3, 1}, {3, 2}, {3, 3},
        // front face
        {4, 0}, {4, 1}, {4, 2}, {4, 3},
        // back face
        {5, 0}, {5, 1}, {5, 2}, {5, 3},
    },
    .indices = {
        // top face
        0, 1, 2, 2, 3, 0,
        // left face
        4, 5, 6, 6, 7, 4,
        // right face
        8, 9, 10, 10, 11, 8,
        // front face
        12, 13, 14, 14, 15, 12,
        // back face
        16, 17, 18, 18, 19, 16,
    }
};

Torch *Torch::gShared = nullptr;

/**
 * Registers the torch block type.
 */
void Torch::Register() {
    gShared = new Torch;
    BlockRegistry::registerBlock(gShared->getId(), dynamic_cast<Block *>(gShared));
}

/**
 * Sets up the block type and registers its textures.
 */
Torch::Torch() {
    using Type = BlockRegistry::TextureType;

    // set id and name
    this->internalName = "me.tseifert.cubeland.block.torch";
    this->id = uuids::uuid::from_string("0ACDFBDF-9B26-459D-AA4A-5D09FEB25C94");

    // register textures
    this->sideTexture = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(8, 32), [](auto &out) {
        TextureLoader::load("block/torch/side.png", out);
    });
    this->topTexture = BlockRegistry::registerTexture(Type::kTypeBlockFace,
            glm::ivec2(8, 8), [](auto &out) {
        TextureLoader::load("block/torch/top.png", out);
    });

    this->inventoryIcon = BlockRegistry::registerTexture(Type::kTypeInventory,
            glm::ivec2(96, 96), [](auto &out) {
        TextureLoader::load("block/torch/inventory.png", out);
    });

    // and register the model
    this->modelVertical = BlockRegistry::registerModel(kVerticalModel);

    // register appearance
    this->appearanceId = BlockRegistry::registerBlockAppearance();
    BlockRegistry::appearanceSetTextures(this->appearanceId, this->topTexture, this->topTexture,
            this->sideTexture);
}


/**
 * Returns the default torch block appearance.
 */
uint16_t Torch::getBlockId(const glm::ivec3 &pos, const BlockFlags flags) {
    return this->appearanceId;
}

/**
 * Returns the appropriate model based on the blocks which this torch is adjacent to.
 */
uint16_t Torch::getModelId(const glm::ivec3 &pos, const BlockFlags flags) {
    // TODO: select correct model
    return this->modelVertical;
}

/**
 * Gets the selection transform matrix for the torch.
 */
glm::mat4 Torch::getSelectionTransform(const glm::ivec3 &pos) {
    const auto trans = translate(glm::mat4(1), glm::vec3(0, -(1-.74)/2., 0));
    return scale(trans, glm::vec3(.2, .74, .2));
}


/**
 * Adds chunk observers on load such that we can determine when torches are removed.
 */
void Torch::chunkWasLoaded(std::shared_ptr<Chunk> chunk) {
    const auto &chunkPos = chunk->worldPos;

    // add change handlers
    using namespace std::placeholders;
    const auto token = chunk->registerChangeCallback(std::bind(&Torch::blockDidChange, this,
                _1, _2, _3, _4));

    std::lock_guard<std::mutex> lg(this->chunkObserversLock);
    this->chunkObservers[chunkPos] = token;
}

/**
 * Remove particle systems for all torches in the given chunk.
 */
void Torch::chunkWillUnload(std::shared_ptr<Chunk> chunk) {
    const auto &chunkPos = chunk->worldPos;

    // remove change handler
    std::lock_guard<std::mutex> lg(this->chunkObserversLock);

    if(this->chunkObservers.contains(chunkPos)) {
        chunk->unregisterChangeCallback(this->chunkObservers[chunkPos]);
        this->chunkObservers.erase(chunkPos);
    }
}

/**
 * Create torch particle systems (as needed) when torches are loaded into the world
 */
void Torch::blockWillDisplay(const glm::ivec3 &pos) {
    std::lock_guard<std::mutex> lg(this->infoLock);
    this->addedTorch(pos);
}

/**
 * Chunk change callback
 */
void Torch::blockDidChange(world::Chunk *chunk, const glm::ivec3 &blockCoord, const world::Chunk::ChangeHints hints, const uuids::uuid &blockId) {
    std::lock_guard<std::mutex> lg(this->infoLock);

    // check adjacent blocks to see if they're a torch and should be yeeted
    if(hints & Chunk::ChangeHints::kBlockRemoved) {
        // check above
        const auto above = blockCoord + glm::ivec3(0, 1, 0);
        if(chunk->getBlock(above) == this->id) {
            const auto pos = above + glm::ivec3(chunk->worldPos.x * 256, 0, chunk->worldPos.y * 256);

            // remove torch and add to inventory
            this->removedTorch(pos);

            chunk->setBlock(above, BlockRegistry::kAirBlockId, true, false);
            this->addInventoryItem(this->id, 1);
        }
    }

    // ignore all non-torch blocks
    if(blockId != this->id) return;

    auto worldPos = blockCoord;
    worldPos += glm::ivec3(chunk->worldPos.x * 256, 0, chunk->worldPos.y * 256);

    // if a torch was added, create its particle system
    if(hints & Chunk::ChangeHints::kBlockAdded) {
        this->addedTorch(worldPos);
    }
    // a torch was removed
    else if(hints & Chunk::ChangeHints::kBlockRemoved) {
        this->removedTorch(worldPos);
    }
}



/**
 * Creates a torch's particle system when it appears, if it doesn't exist already.
 *
 * @note Assumes we're holding the torch info lock.
 */
void Torch::addedTorch(const glm::ivec3 &worldPos) {
    // bail if we've already got torch info for that position
    if(this->info.contains(worldPos)) {
        return;
    }

    // create particle system
    const auto particleOrigin = glm::vec3(worldPos) + glm::vec3(.5, .8, .5);

    auto sys = std::make_shared<TorchSmoke>(particleOrigin);
    this->addParticleSystem(std::dynamic_pointer_cast<particles::System>(sys));

    // create its light
    auto light = std::make_shared<gfx::PointLight>();
    light->setPosition(particleOrigin + glm::vec3(0, .15, 0));
    light->setColors(kLightColor, glm::vec3(.4, .4, .4));
    light->setLinearAttenuation(kLinearAttenuation);
    light->setQuadraticAttenuation(kQuadraticAttenuation);

    this->addLight(std::dynamic_pointer_cast<gfx::lights::AbstractLight>(light));

    // build info struct and save it
    Info i;

    i.smoke = sys;
    i.light = light;

    this->info[worldPos] = i;
}

/**
 * Removes a torch's particle systems when removed.
 *
 * @note Assumes we're holding the torch info lock.
 */
void Torch::removedTorch(const glm::ivec3 &worldPos) {
    // bail if we've not got torch info at that position
    if(!this->info.contains(worldPos)) {
        Logging::error("Removing torch at {} with no torch info!", worldPos);
        return;
    }

    // remove the particle system
    auto &i = this->info[worldPos];

    this->removeParticleSystem(i.smoke);
    this->removeLight(std::dynamic_pointer_cast<gfx::lights::AbstractLight>(i.light));

    // remove the info object
    this->info.erase(worldPos);
}

