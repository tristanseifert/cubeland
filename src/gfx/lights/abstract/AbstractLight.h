/*
 * AbstractLight.h
 *
 * An abstract base class representing a generic light, holding the different
 * pieces of information needed to render most kinds of lights.
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#ifndef GFX_LEVEL_PRIMITIVES_ABSTRACT_LIGHT_H_
#define GFX_LEVEL_PRIMITIVES_ABSTRACT_LIGHT_H_

#include <glm/glm.hpp>

#include <string>
#include <memory>

namespace gfx {
	// forward-declare a class for compilation
	class ShaderProgram;

namespace lights {
class AbstractLight {
    public:
        enum LightType {
            Ambient = 1,
            Directional,
            Point,
            Spot,

            Unknown = -1
        };

    public:
        AbstractLight();
        virtual ~AbstractLight();

        virtual void sendToProgram(int index, std::shared_ptr<gfx::ShaderProgram> program) = 0;

        void setDiffuseColor(const glm::vec3 diffuse);
        glm::vec3 getDiffuseColor(void) const {
            return this->diffuseColor;
        }

        void setSpecularColor(const glm::vec3 specular);
        glm::vec3 getSpecularColor(void) const {
            return this->specularColor;
        }

        /**
         * Sets the diffuse and specular colors.
         */
        void setColors(glm::vec3 diffuse, glm::vec3 specular) {
            this->setDiffuseColor(diffuse);
            this->setSpecularColor(specular);
        }

        /**
         * Sets the color of the diffuse and specular to the same value.
         */
        void setColor(glm::vec3 color) {
            this->setDiffuseColor(color);
            this->setSpecularColor(color);
        }

        /// gets the type of this light
        LightType getType(void) {
            return this->type;
        }

    protected:
        void sendColors(int i, std::shared_ptr<ShaderProgram> program, const std::string &array);

    protected:
        LightType type = AbstractLight::Unknown;

    private:
        glm::vec3 diffuseColor;
        glm::vec3 specularColor;
};
} /* namespace lights */
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_ABSTRACT_LIGHT_H_ */
