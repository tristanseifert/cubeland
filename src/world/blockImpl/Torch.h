#ifndef WORLD_BLOCKIMPL_TORCH_H
#define WORLD_BLOCKIMPL_TORCH_H

#include "world/block/Block.h"
#include "world/block/BlockRegistry.h"
#include "world/chunk/Chunk.h"

#include <memory>
#include <unordered_map>
#include <mutex>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/hash.hpp>

namespace particles {
class System;
}

namespace gfx {
class PointLight;
};

namespace world::blocks {
class Torch: public Block {
    public:
        static void Register();

    Torch();

    public:
        uint16_t getBlockId(const glm::ivec3 &pos, const BlockFlags flags) override;
        uint16_t getModelId(const glm::ivec3 &pos, const BlockFlags flags) override;

        const std::string getDisplayName() const override {
            return "Torch";
        }

        /// Torch has blank spaces so it's not fully opaque
        const bool isOpaque() const override {
            return false;
        }
        /// Cannot be collided with
        const bool isCollidable(const glm::ivec3 &pos) const override {
            return false;
        }

        /// Torches drop instantly
        const size_t destroyTicks(const glm::ivec3 &pos) const override {
            return 0;
        }

        /// Use the chunk unloading notification to remove particle systems
        const bool wantsChunkLoadNotifications() const override { return true; }
        /// Add observers to each chunk such that we can notice when a torch is removed
        void chunkWasLoaded(std::shared_ptr<Chunk> chunk) override;
        /// Remove torch particle systems when their chunk unloads
        void chunkWillUnload(std::shared_ptr<Chunk> chunk) override;
        /// when a torch is first yeeted into the world, create particle systems
        void blockWillDisplay(const glm::ivec3 &pos) override;

        /// allow a different sized selection
        glm::mat4 getSelectionTransform(const glm::ivec3 &pos) override;
    private:
        void blockDidChange(world::Chunk *, const glm::ivec3 &, const world::Chunk::ChangeHints,
                const uuids::uuid &);

        void addedTorch(const glm::ivec3 &worldPos);
        void removedTorch(const glm::ivec3 &worldPos);

    private:
        static Torch *gShared;

        static const BlockRegistry::Model kVerticalModel;

    private:
        /// linear attenuation of torch point light
        constexpr static const float kLinearAttenuation = .035;
        /// quadratic attenuation of a torch point light
        constexpr static const float kQuadraticAttenuation = .0088;

        /// color for the point light; it's slightly orange-ish
        constexpr static const glm::vec3 kLightColor = glm::vec3(1.15, .8, .8);

    private:
        /// Holds all auxiliary info for a single torch
        struct Info {
            // smoke particle system
            std::shared_ptr<particles::System> smoke = nullptr;
            // light secreted by the torch
            std::shared_ptr<gfx::PointLight> light = nullptr;
        };

    private:
        /// textures for side and top
        BlockRegistry::TextureId sideTexture, topTexture;
        /// material textures
        BlockRegistry::TextureId sideMat, topMat;
        /// type id for the primary block appearance
        uint16_t appearanceId;

        /// model id for the vertical torch
        uint16_t modelVertical = 0;

        /// registered chunk change observers
        std::unordered_map<glm::ivec2, uint32_t> chunkObservers;
        /// lock for chunk change observers
        std::mutex chunkObserversLock;

        /// all torches we've loaded
        std::unordered_map<glm::ivec3, Info> info;
        /// lock for the torch info map
        std::mutex infoLock;
};
}

#endif
