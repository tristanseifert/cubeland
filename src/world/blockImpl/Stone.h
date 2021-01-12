#ifndef WORLD_BLOCKIMPL_STONE_H
#define WORLD_BLOCKIMPL_STONE_H

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"

#include <memory>

namespace world::blocks {
class Stone: public Block {
    public:
        const static uuids::uuid kBlockId;

    public:
        static void Register();

        Stone();

    public:
        uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) override;

        const std::string getDisplayName() const override {
            return "Stone";
        }

        const size_t destroyTicks(const glm::ivec3 &pos) const override {
            return 25;
        }

    private:
        static Stone *gShared;

        /// texture (all faces have the same texture)
        BlockRegistry::TextureId diffuse;
        /// mateiral properties
        BlockRegistry::TextureId material;
        /// type id for the primary dirt block appearance
        uint16_t appearanceId;
};
}

#endif
