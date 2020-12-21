/*
 * PointLight.h
 *
 * A point light, which is modeled as a sphere that radiates light outwards.
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_POINTLIGHT_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_POINTLIGHT_H_

#include <memory>

#include "abstract/AbstractLight.h"
#include "abstract/LightAttenuation.h"
#include "abstract/LightPosition.h"

namespace gfx {
class PointLight: public lights::LightPosition,
        public lights::LightAttenuation,
        public lights::AbstractLight {
    public:
        PointLight();
        virtual ~PointLight() = default;

        void sendToProgram(const int index, std::shared_ptr<ShaderProgram> program);
};
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_POINTLIGHT_H_ */
