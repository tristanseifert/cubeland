/*
 * LightAttenuation.cpp
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#include "LightAttenuation.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "io/Format.h"

using namespace gfx::lights;

/**
 * Sets the linear attenuation. This has the largest effect at close range.
 */
void LightAttenuation::setLinearAttenuation(float attenuation) {
	this->linearAttenuation = attenuation;
}

/**
 * Sets the quadratic attenuation. As the distance increases, this will have a
 * much larger effect.
 */
void LightAttenuation::setQuadraticAttenuation(float attenuation) {
	this->quadraticAttenuation = attenuation;
}

/**
 * Sends the attenuation parameters to the GPU.
 */
void LightAttenuation::sendAttenuation(int i, std::shared_ptr<gfx::ShaderProgram> program, const std::string &array) {
    program->setUniform1f(f("{}[{:d}].Linear", array, i), this->getLinearAttenuation());
    program->setUniform1f(f("{}[{:d}].Quadratic", array, i), this->getQuadraticAttenuation());
}
