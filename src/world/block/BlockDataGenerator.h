#ifndef WORLD_BLOCK_BLOCKDATAGENERATOR_H
#define WORLD_BLOCK_BLOCKDATAGENERATOR_H

#include <vector>
#include <cstddef>
#include <array>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace world {
class BlockRegistry;
class Block;

class BlockDataGenerator {
    public:
        /// number of columns (width) of the data texture
        constexpr static const size_t kDataColumns = 16;

    public:
        BlockDataGenerator(BlockRegistry *_reg) : registry(_reg) {};

    public:
        void buildTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
        void generate(glm::ivec2 &size, std::vector<glm::vec4> &out);

    private:
        static const std::array<glm::vec2, 4> kFaceUv;

    private:
        void writeBlockInfo(std::vector<glm::vec4> &out, const size_t y, const std::shared_ptr<Block> &block);

    private:
        /// this is our data source for all block data
        BlockRegistry *registry = nullptr;
};
}

#endif
