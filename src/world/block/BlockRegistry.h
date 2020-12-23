/**
 * The block registry stores the behaviors of all blocks, info on how to draw them, and so forth.
 * This allows additional blocks to be defined at later times.
 */
#ifndef WORLD_BLOCK_BLOCKREGISTRY_H
#define WORLD_BLOCK_BLOCKREGISTRY_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

#include <uuid.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace world {
class Block;
class BlockDataGenerator;

class BlockRegistry {
    friend class BlockDataGenerator;

    public:
        const static uuids::uuid kAirBlockId;

        /// All textures registered by blocks to be included in the texture maps use these IDs
        using TextureId = uint32_t;

    public:
        // you should not call this
        BlockRegistry();
        ~BlockRegistry();

        /// Forces initialization of the block registry
        static void init();
        /// Releases the shared handle to the block registry
        static void shutdown() {
            gShared = nullptr;
        }

        /// Determines whether the given block id is for an air block.
        static bool isAirBlock(const uuids::uuid &id) {
            return (id == kAirBlockId);
        }

        /// Returns the total number of registered blocks
        static size_t getNumRegistered() {
            return gShared->blocks.size();
        }

        /// Registers a primary block responsible for handling the given UUID
        static void registerBlock(const uuids::uuid &id, std::shared_ptr<Block> block);
        /// Gets a handle to a registered block instance
        static std::shared_ptr<Block> &getBlock(const uuids::uuid &id);

        /// Registers a texture
        static TextureId registerTexture(const glm::ivec2 size, const std::function<void(std::vector<float> &)> &fillFunc);
        /// Removes an existing texture registration
        static void removeTexture(const TextureId id);

        /// Registers a new block appearance. This initially is blank
        static uint16_t registerBlockAppearance();
        /// Removes a previously registered block appearance
        static void removeBlockAppearance(const uint16_t id);
        /// Sets the texture IDs used by a block appearance
        static void appearanceSetTextures(const uint16_t id, const TextureId top, const TextureId bottom, const TextureId side);
        /// Sets the texture IDs for all three faces from an array.
        static void appearanceSetTextures(const uint16_t id, const TextureId ids[3]) {
            appearanceSetTextures(id, ids[0], ids[1], ids[2]);
        }

        /// generates the block texture atlas
        static void generateBlockTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
        /// generates the block info data
        static void generateBlockData(glm::ivec2 &size, std::vector<glm::vec4> &out);

    private:

    private:
        /// A texture to be included in the block atlas
        struct TextureReg {
            /// ID of this registration
            TextureId id;
            /// Size of the texture, in pixels
            glm::ivec2 size;

            /**
             * Function to invoke to get the texture data from the block handler for the texture;
             * the data is considered to be in RGBA format, tightly packed, in the provided
             * vector.
             *
             * The given output buffer is resized to exactly width * height * 4 elements.
             */
            std::function<void(std::vector<float> &)> fillFunc;
        };

        /// Block implementation wrapper
        struct BlockInfo {
            /// block data structure; defines its behavior and how it appears
            std::shared_ptr<Block> block;
        };

        /// Info for rendering a block
        struct BlockAppearanceType {
            /// Texture IDs for the top, bottom and sides
            TextureId texTop, texBottom, texSide;
        };

    private:
        static std::shared_ptr<BlockRegistry> gShared;

    private:
        /// all registered blocks. key is block UUID
        std::unordered_map<uuids::uuid, BlockInfo> blocks;
        /// lock for the blocks registration map
        std::mutex blocksLock;

        /// all registered appearance types
        std::unordered_map<uint16_t, BlockAppearanceType> appearances;
        /// lock protecting the appearances map
        std::mutex appearancesLock;
        /// last used appaearanc  ID
        uint16_t lastAppearanceId = 1;

        /// all registered texture
        std::unordered_map<TextureId, TextureReg> textures;
        /// lock for the textures map
        std::mutex texturesLock;
        /// last used texture id
        TextureId lastTextureId = 1;

        /// used to generate the block info textures
        BlockDataGenerator *dataGen = nullptr;

};

/**
 * Registers the built-in block types.
 */
void RegisterBuiltinBlocks();
}

#endif
