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
#include <utility>
#include <mutex>

#include <uuid.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace world {
struct Chunk;
class Block;
class BlockDataGenerator;

class BlockRegistry {
    friend class BlockDataGenerator;

    public:
        const static uuids::uuid kAirBlockId;

        /// All textures registered by blocks to be included in the texture maps use these IDs
        using TextureId = uint32_t;

        /// types of textures (e.g. what atlas they end up in)
        enum class TextureType {
            /// textures used to render block edges
            kTypeBlockFace,
            /// Textures used to render the inventory screen
            kTypeInventory,
        };

        /**
         * Blocks can register custom models, which are just vertices whose coordinates are in the
         * range of [0, 1], and an index buffer. Note that the bottom left corner of the block is
         * at the origin.
         *
         * Each vertex position also must correspond to a face/vertex id pair. The faces are
         * ordered as 0 = bottom, 1 = top, 2 = left, 3 = right, 4 = front, 5 = back.
         *
         * A maximum of about 60 vertices is suggested.
         */
        struct Model {
            std::vector<glm::vec3> vertices;
            std::vector<std::pair<uint8_t, uint8_t>> faceVertIds;
            std::vector<uint8_t> indices;
        };

    public:
        // you should not call this
        BlockRegistry();
        ~BlockRegistry();

        /// Forces initialization of the block registry
        static void init();
        /// Releases the shared handle to the block registry
        static void shutdown() {
            delete gShared;
        }

        /// Determines whether the given block id is for an air block.
        static bool isAirBlock(const uuids::uuid &id) {
            return (id == kAirBlockId);
        }
        /// Determines whether a block can be collided with
        static bool isCollidableBlock(const uuids::uuid &id, const glm::ivec3 &pos);
        /// Determines whether a block is fully opaque.
        static bool isOpaqueBlock(const uuids::uuid &id);
        /// Determines whether the given block can be selected.
        static bool isSelectable(const uuids::uuid &id, const glm::ivec3 &pos);

        /// Returns the total number of registered blocks
        static size_t getNumRegistered() {
            return gShared->blocks.size();
        }

        /// Registers a primary block responsible for handling the given UUID
        static void registerBlock(const uuids::uuid &id, Block *block);
        /// Gets a handle to a registered block instance
        static Block *getBlock(const uuids::uuid &id);
        /// Invokes a method for each registered block.
        static void iterateBlocks(const std::function<void(const uuids::uuid &, Block *)> &cb);

        /// Notifies all interested blocks that a chunk has loaded.
        static void notifyChunkLoaded(std::shared_ptr<Chunk> &ptr);
        /// Notifies all interested blocks that a chunk is about to be unloaded.
        static void notifyChunkWillUnload(std::shared_ptr<Chunk> &ptr);

        /// Registers a texture
        static TextureId registerTexture(const TextureType type, const glm::ivec2 size, const std::function<void(std::vector<float> &)> &fillFunc);
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

        /// Registers a new model.
        static uint16_t registerModel(const Model &mod);
        /// Checks whether the given model exists.
        static const bool hasModel(const uint16_t modelId) {
            return gShared->models.contains(modelId);
        }
        /// Returns a reference to the given model.
        static const Model &getModel(const uint16_t modelId) {
            return gShared->models[modelId];
        }

        /// Returns UV coordinates for the given texture.
        static glm::vec4 getTextureUv(const TextureId id);

        /// generates the block texture atlas
        static void generateBlockTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
        /// generates the inventory texture atlas
        static void generateInventoryTextureAtlas(glm::ivec2 &size, std::vector<std::byte> &out);
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
            /// intended use of the texture (e.g. what atlas it ends up in)
            TextureType type;

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
            Block *block;
        };

        /// Info for rendering a block
        struct BlockAppearanceType {
            /// Texture IDs for the top, bottom and sides
            TextureId texTop, texBottom, texSide;
        };

    private:
        static BlockRegistry *gShared;

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

        /// registered models
        std::unordered_map<uint16_t, Model> models;
        /// lock protecting models
        std::mutex modelsLock;
        /// last used model id
        uint16_t lastModelId = 1;

        /// used to generate the block info textures
        BlockDataGenerator *dataGen = nullptr;

};

/**
 * Registers the built-in block types.
 */
void RegisterBuiltinBlocks();
}

#endif
