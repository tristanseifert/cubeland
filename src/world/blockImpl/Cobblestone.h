#ifndef WORLD_BLOCKIMPL_COBBLESTONE_H
#define WORLD_BLOCKIMPL_COBBLESTONE_H

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"

#include <memory>

namespace world::blocks {
class Cobblestone: public Block {
    public:
        static void Register();

    Cobblestone();

    public:
        uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) override;

        const std::string getDisplayName() const override {
            return "Cobblestone";
        }

        const size_t destroyTicks(const glm::ivec3 &pos) const override {
            return 30;
        }

    private:
        static Cobblestone *gShared;

        /// texture (all faces have the same texture)
        BlockRegistry::TextureId diffuse;
        /// mateiral properties
        BlockRegistry::TextureId material;
        /// surface normals
        BlockRegistry::TextureId normal;

        /// type id for the primary dirt block appearance
        uint16_t appearanceId;
};
}

#endif
