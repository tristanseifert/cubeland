#ifndef WORLD_BLOCK_BLOCKDATAGENERATOR_H
#define WORLD_BLOCK_BLOCKDATAGENERATOR_H

#include "BlockRegistry.h"

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
        constexpr static const size_t kDataColumns = 16;

    public:
        BlockDataGenerator(BlockRegistry *_reg) : registry(_reg) {};

    public:
        void buildTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
        void repackAtlas();

        void generate(glm::ivec2 &size, std::vector<glm::vec4> &out);

        glm::vec4 uvBoundsForTexture(BlockRegistry::TextureId id);

    private:
        static const std::array<glm::vec2, 4> kFaceUv;

    private:
        void buildAtlasLayout();

        void writeBlockInfo(std::vector<glm::vec4> &out, const size_t y, const BlockRegistry::BlockAppearanceType &block);

    private:
        /// this is our data source for all block data
        BlockRegistry *registry = nullptr;

        /// mapping of texture id -> bounding rect in the texture atlas
        std::unordered_map<BlockRegistry::TextureId, glm::ivec4> atlasLayout;
        /// size of the texture atlas
        glm::ivec2 atlasSize;

        /// when set, the texture atlas is generated as 16-bit float rather than 8-bit unsigned
        bool makeFloatAtlas = true;
};
}

#endif
