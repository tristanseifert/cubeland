/*
 * DirectionalLight.cpp
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#include "DirectionalLight.h"

using namespace gfx;

/**
 * Sets the default light type.
 */
DirectionalLight::DirectionalLight() {
	this->type = lights::AbstractLight::Directional;
}

/**
 * Sends the uniforms required to render this light to the lighting shader.
 *
 * @note Assumes the standard light structures, as defined in `lighting.shader.`
 */
void DirectionalLight::sendToProgram(const int i, std::shared_ptr<ShaderProgram> program) {
	this->sendDirection(i, program, "directionalLights");
	this->sendColors(i, program, "directionalLights");
}
