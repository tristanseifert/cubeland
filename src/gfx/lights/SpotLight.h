/*
 * SpotLight.h
 *
 * A spot light, which casts light within a circle of a specific radius, as
 * defined in degrees.
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_SPOTLIGHT_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_SPOTLIGHT_H_

#include <memory>

#include "abstract/AbstractLight.h"
#include "abstract/LightAttenuation.h"
#include "abstract/LightDirection.h"
#include "abstract/LightPosition.h"

namespace gfx {
class SpotLight: public lights::AbstractLight,
        public lights::LightAttenuation,
        public lights::LightDirection,
        public lights::LightPosition {
    public:
        SpotLight();
        virtual ~SpotLight() {};

        void sendToProgram(const int index, std::shared_ptr<ShaderProgram> program);

        void setInnerCutOff(float cutoff);
        float getInnerCutOff(void) {
            return this->innerCutOff;
        }

        void setOuterCutOff(float cutoff);
        float getOuterCutOff(void) {
            return this->outerCutOff;
        }

    private:
        /// the inner cutoff is the point where light begins to fade…
        float innerCutOff;
        /// …and the outer cutoff is where the light stops completely.
        float outerCutOff;
};
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_SPOTLIGHT_H_ */
