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

namespace render {
class Lighting;
}

namespace gfx {
// forward-declare a class for compilation
class ShaderProgram;

namespace lights {
class AbstractLight {
    friend class render::Lighting;

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
        LightType getType(void) const {
            return this->type;
        }

        /// sets whether this light is enabled, e.g. whether it's sent to the lighting shader
        void setEnabled(const bool v) {
            this->lightEnabled = v;
        }
        /// returns whether the light is enabled
        bool isEnabled() const {
            return this->lightEnabled;
        }

        /// returns the dirty flag state
        bool isDirty() const {
            return this->dirty;
        }

    protected:
        void sendColors(int i, std::shared_ptr<ShaderProgram> program, const std::string &array);

        virtual void markDirty() {
            this->dirty = true;
        }

    protected:
        LightType type = AbstractLight::Unknown;
        // set whenever any light parameters are changed. should be cleared when sent to program
        bool dirty = true;

    private:
        glm::vec3 diffuseColor;
        glm::vec3 specularColor;

        bool lightEnabled = true;
};
} /* namespace lights */
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_ABSTRACT_LIGHT_H_ */
