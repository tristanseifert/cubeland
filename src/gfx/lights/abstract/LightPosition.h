/*
 * Represents a light that has a position attribute.
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTPOSITION_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTPOSITION_H_

#include <glm/glm.hpp>

#include <memory>
#include <string>

namespace render {
class Lighting;
}

namespace gfx {
class ShaderProgram;

namespace lights {
class LightPosition {
    friend class render::Lighting;

    public:
        LightPosition();

        void setPosition(glm::vec3 position);
        glm::vec3 getPosition() const {
            return this->position;
        }

    protected:
        void sendPosition(int i, std::shared_ptr<gfx::ShaderProgram> program, const std::string &array);
        virtual void markDirty() = 0;

    private:
        glm::vec3 position;
};
} /* namespace lights */
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTPOSITION_H_ */
