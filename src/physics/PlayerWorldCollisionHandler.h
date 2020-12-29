/*
 * Provides interface to check whether a proposed player position is valid.
 */
#ifndef PHYSICS_PLAYERWORLDCOLLISIONHANDLER_H
#define PHYSICS_PLAYERWORLDCOLLISIONHANDLER_H

#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace render {
class SceneRenderer;
}

namespace physics {
class PlayerWorldCollisionHandler {
    public:
        PlayerWorldCollisionHandler(std::shared_ptr<render::SceneRenderer> &renderer);

        bool isPositionOk(const glm::vec3 &pos);

    private:
        /// get chunk data from this scene
        std::shared_ptr<render::SceneRenderer> scene = nullptr;
};
}

#endif
