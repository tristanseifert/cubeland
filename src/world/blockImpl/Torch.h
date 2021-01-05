#ifndef WORLD_BLOCKIMPL_TORCH_H
#define WORLD_BLOCKIMPL_TORCH_H

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"

#include <memory>

namespace world::blocks {
class Torch: public Block {
    public:
        static void Register();

    Torch();

    public:
        uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) override;
        uint16_t getModelId(const glm::ivec3 &pos, const BlockFlags flags) override;

        /// Torch has blank spaces so it's not fully opaque
        const bool isOpaque() const override {
            return false;
        }
        /// Cannot be collided with
        const bool isCollidable(const glm::ivec3 &pos) const override {
            return false;
        }

        const size_t destroyTicks(const glm::ivec3 &pos) const override {
            return 0;
        }

    private:
        static Torch *gShared;

        static const BlockRegistry::Model kVerticalModel;

    private:

        /// texture (for now, just side textures)
        BlockRegistry::TextureId texture;
        /// type id for the primary block appearance
        uint16_t appearanceId;

        /// model id for the vertical torch
        uint16_t modelVertical = 0;
};
}

#endif
