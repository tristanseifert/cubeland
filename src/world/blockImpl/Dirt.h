#ifndef WORLD_BLOCKIMPL_DIRT_H
#define WORLD_BLOCKIMPL_DIRT_H

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"

#include <memory>

namespace world::blocks {
class Dirt: public Block {
    public:
        static void Register();

        Dirt();

    public:
        uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) override;

    private:
        static std::shared_ptr<Dirt> gShared;

        /// textures
        BlockRegistry::TextureId normalTextures[3];
        /// type id for the primary dirt block appearance
        uint16_t appearanceId;
        /// dirt with no grass (only the bottom texture)
        uint16_t noGrassAppearance;
};
}

#endif
