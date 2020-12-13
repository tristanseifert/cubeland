/*
 * LightAttenuation.h
 *
 * Represents a light that attenuates over distance, based on a combined linear-
 * quadratic model.
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTATTENUATION_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTATTENUATION_H_

#include <string>
#include <memory>

namespace gfx {
class ShaderProgram;

namespace lights {
class LightAttenuation {
    public:
        LightAttenuation() {};
        virtual ~LightAttenuation() {};

        void setLinearAttenuation(float attenuation);
        float getLinearAttenuation(void) {
            return this->linearAttenuation;
        }

        void setQuadraticAttenuation(float attenuation);
        float getQuadraticAttenuation(void) {
            return this->quadraticAttenuation;
        }

    protected:
        void sendAttenuation(int i, std::shared_ptr<gfx::ShaderProgram> program, const std::string &array);

    private:
        float linearAttenuation = 0.7f;
        float quadraticAttenuation = 1.8f;
};
} /* namespace lights */
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_ABSTRACT_LIGHTATTENUATION_H_ */
