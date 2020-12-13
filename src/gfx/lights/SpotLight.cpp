/*
 * SpotLight.cpp
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#include "SpotLight.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "io/Format.h"

using namespace gfx;

/**
 * Sets the default light type.
 */
SpotLight::SpotLight() {
	this->type = lights::AbstractLight::Spot;

	// set the default cut offs (in degrees)
	this->innerCutOff = 12.5f;
	this->outerCutOff = 17.5f;

	// default colours
	this->setColor(glm::vec3(1));
}

/**
 * Sends the uniforms required to render this light to the lighting shader.
 *
 * @note Assumes the standard light structures, as defined in `lighting.shader.`
 */
void SpotLight::sendToProgram(const int i, std::shared_ptr<ShaderProgram> program) {
	this->sendPosition(i, program, "spotLights");
	this->sendDirection(i, program, "spotLights");

	this->sendColors(i, program, "spotLights");

	// cut-off angles and attenuation
    program->setUniform1f(f("spotLights[{:d}].InnerCutOff", i),
    						glm::cos(glm::radians(this->innerCutOff)));
    program->setUniform1f(f("spotLights[{:d}].OuterCutOff", i),
    						glm::cos(glm::radians(this->outerCutOff)));

    this->sendAttenuation(i, program, "spotLights");
}

/**
 * Sets the inner cutoff angle. Once past this angle, the spotlight will lose
 * intensity.
 */
void SpotLight::setInnerCutOff(float cutoff) {
	this->innerCutOff = cutoff;
}

/**
 * Sets the outer cutoff angle. Outside of this angle, no light is cast.
 */
void SpotLight::setOuterCutOff(float cutoff) {
	this->outerCutOff = cutoff;
}
