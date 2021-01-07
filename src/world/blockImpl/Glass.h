#ifndef WORLD_BLOCKIMPL_GLASS_H
#define WORLD_BLOCKIMPL_GLASS_H

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"

#include <memory>

namespace world::blocks {
class Glass: public Block {
    public:
        static void Register();

        Glass();

    public:
        const std::string getDisplayName() const override {
            return "Glass (Solid)";
        }

        // we have but one appearance
        uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) override {
            return this->appearanceId;
        }
        // it is not opaque (affects adjacent block face generation)
        const bool isOpaque() const override {
            return false;
        }
        // always use the special blending step since that turns off face culling
        const bool needsAlphaBlending(const glm::ivec3 &pos) const override {
            return true;
        }

        // drop instantly
        const size_t destroyTicks(const glm::ivec3 &pos) const override {
            return 0;
        }
        // but do not be collectable
        const bool isCollectable(const glm::ivec3 &pos) const override {
            return false;
        }

    private:
        static Glass *gShared;

        /// texture (all faces have the same texture)
        BlockRegistry::TextureId diffuse;
        /// mateiral properties
        BlockRegistry::TextureId material;
        /// type id for the primary dirt block appearance
        uint16_t appearanceId;
};
}

#endif
