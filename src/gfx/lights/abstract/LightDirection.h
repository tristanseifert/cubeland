/*
 * Represents a light that has a direction attribute.
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTDIRECTION_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTDIRECTION_H_

#include <glm/glm.hpp>

#include <string>
#include <memory>

namespace render {
class Lighting;
}

namespace gfx {
class ShaderProgram;

namespace lights {
class LightDirection {
    friend class render::Lighting;

    public:
        LightDirection();

        void setDirection(glm::vec3 direction);
        glm::vec3 getDirection() const {
            return this->direction;
        }

    protected:
        void sendDirection(int i, std::shared_ptr<gfx::ShaderProgram> program, const std::string &array);
        virtual void markDirty() = 0;

    private:
        glm::vec3 direction;
};
} /* namespace lights */
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTDIRECTION_H_ */
