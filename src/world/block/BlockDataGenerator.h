#ifndef WORLD_BLOCK_BLOCKDATAGENERATOR_H
#define WORLD_BLOCK_BLOCKDATAGENERATOR_H

#include "BlockRegistry.h"

#include "util/TexturePacker.h"

#include <vector>
#include <cstddef>
#include <array>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace world {
class Block;

class BlockDataGenerator {
    public:
        /// number of columns (width) of the data texture
        constexpr static const size_t kDataColumns = 48;

    public:
        BlockDataGenerator(BlockRegistry *_reg) : registry(_reg) {};

    public:
        void buildBlockTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
        void repackBlockAtlas();
        void buildBlockMaterialTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
        void buildBlockNormalTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);

        void buildInventoryTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);

        void generate(glm::ivec2 &size, std::vector<glm::vec4> &out);

        glm::vec4 uvBoundsForBlockTexture(BlockRegistry::TextureId id) {
            return this->blockAtlas.uvBoundsForTexture(id);
        }
        glm::vec4 uvBoundsForMaterialTexture(BlockRegistry::TextureId id) {
            return this->blockMaterialAtlas.uvBoundsForTexture(id);
        }
        glm::vec4 uvBoundsForNormalTexture(BlockRegistry::TextureId id) {
            return this->blockNormalAtlas.uvBoundsForTexture(id);
        }
        glm::vec4 uvBoundsForInventoryTexture(BlockRegistry::TextureId id) {
            return this->inventoryAtlas.uvBoundsForTexture(id);
        }

    private:
        static const std::array<glm::vec2, 4> kFaceUv;

    private:
        void writeBlockInfo(std::vector<glm::vec4> &out, const size_t y, const BlockRegistry::BlockAppearanceType &block);

        void writeDiffuseUv(std::vector<glm::vec4> &out, const size_t off, const BlockRegistry::BlockAppearanceType &block);
        void writeMaterialUv(std::vector<glm::vec4> &out, const size_t off, const BlockRegistry::BlockAppearanceType &block);
        void writeNormalUv(std::vector<glm::vec4> &out, const size_t off, const BlockRegistry::BlockAppearanceType &block);

        void copyAtlas(const util::TexturePacker<BlockRegistry::TextureId> &, glm::ivec2 &, std::vector<std::byte> &, const size_t ncomps = 4);

    private:
        /// this is our data source for all block data
        BlockRegistry *registry = nullptr;

        /// texture packer for block textures
        util::TexturePacker<BlockRegistry::TextureId> blockAtlas;
        /// whether the block atlas needs to be updated
        bool forceBlockAtlasUpdate = true;

        /// texture packer for block material textures
        util::TexturePacker<BlockRegistry::TextureId> blockMaterialAtlas;
        /// whether the block material atlas needs to be updated
        bool forceBlockMaterialAtlasUpdate = true;

        /// texture packer for block normal textures
        util::TexturePacker<BlockRegistry::TextureId> blockNormalAtlas;
        /// whether the block normal atlas needs to be updated
        bool forceBlockNormalAtlasUpdate = true;

        /// texture packer for inventory textures
        util::TexturePacker<BlockRegistry::TextureId> inventoryAtlas;
        /// whether the inventory atlas needs to be updated
        bool forceInventoryAtlasUpdate = true;
};
}

#endif
