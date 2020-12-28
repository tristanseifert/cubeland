/**
 * Handles interactions with blocks in the game; specifically, the ability to place/destroy blocks
 * using mouse input.
 */
#ifndef INPUT_BLOCKINTERACTIONS_H
#define INPUT_BLOCKINTERACTIONS_H

#include <memory>
#include <atomic>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

union SDL_Event;

namespace world {
class WorldSource;
}
namespace render {
class SceneRenderer;
}
namespace inventory {
class Manager;
}

namespace input {

class BlockInteractions {
    public:
        BlockInteractions(std::shared_ptr<render::SceneRenderer> scene, std::shared_ptr<world::WorldSource> source, inventory::Manager *);
        ~BlockInteractions();

        bool handleEvent(const SDL_Event &);

    private:
        void placeBlock();
        void destroyBlock();

        bool allowPlacementAt(const glm::ivec3 &pos);

        void absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos, glm::ivec3 &blockPos);

        void destroyTickCallback();
        void destroyBlockTimerExpired();

        void updateDestroyProgress(bool defer = true);

    private:
        /// Standard block selection color
        constexpr static const glm::vec4 kSelectionColor = glm::vec4(1, 1, 0, .74);
        /// As a block is destroyed, its color slowly advances towards this
        constexpr static const glm::vec4 kSelectionCollectedColor = glm::vec4(0, 1, 0, .74);

    private:
        bool enabled = true;

        inventory::Manager *inventory = nullptr;
        std::shared_ptr<render::SceneRenderer> scene;
        std::shared_ptr<world::WorldSource> source;

        std::atomic_bool destroyTimerActive = false;
        size_t destroyTimer = 0, destroyTimerTotal = 0;
        std::pair<glm::ivec3, glm::ivec3> destroyPos;

        uint32_t tickCb = 0;
};

}

#endif
