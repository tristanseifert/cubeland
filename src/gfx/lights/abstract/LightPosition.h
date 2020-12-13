/*
 * LightPosition.h
 *
 * Represents a light that has a position attribute.
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTPOSITION_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTPOSITION_H_

#include <glm/glm.hpp>

#include <memory>
#include <string>

namespace gfx {
class ShaderProgram;

namespace lights {
class LightPosition {
    public:
        LightPosition();
        virtual ~LightPosition();

        void setPosition(glm::vec3 position);
        glm::vec3 getPosition() const {
            return this->position;
        }

    protected:
        void sendPosition(int i, std::shared_ptr<gfx::ShaderProgram> program, const std::string &array);

    private:
        glm::vec3 position;
};
} /* namespace lights */
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTPOSITION_H_ */
