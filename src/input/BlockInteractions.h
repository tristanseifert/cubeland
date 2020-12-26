/**
 * Handles interactions with blocks in the game; specifically, the ability to place/destroy blocks
 * using mouse input.
 */
#ifndef INPUT_BLOCKINTERACTIONS_H
#define INPUT_BLOCKINTERACTIONS_H

#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

union SDL_Event;

namespace render {
class SceneRenderer;
}

namespace input {

class BlockInteractions {
    public:
        BlockInteractions(std::shared_ptr<render::SceneRenderer> scene);

        bool handleEvent(const SDL_Event &);

    private:
        void placeBlock();
        void destroyBlock();

        bool allowPlacementAt(const glm::ivec3 &pos);

        void absoluteToRelative(const glm::ivec3 &pos, glm::ivec2 &chunkPos, glm::ivec3 &blockPos);

    private:
        bool enabled = true;

        std::shared_ptr<render::SceneRenderer> scene;
};

}

#endif
