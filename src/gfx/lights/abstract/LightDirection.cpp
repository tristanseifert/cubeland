/*
 * LightDirection.cpp
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#include "LightDirection.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "io/Format.h"

using namespace gfx::lights;

/**
 * Sets up a default direction.
 */
LightDirection::LightDirection() {
	this->direction = glm::vec3(0, 1, 0);
}

/**
 * Sets the direction that this light is pointing.
 */
void LightDirection::setDirection(glm::vec3 direction) {
	this->direction = direction;
}

/**
 * Sends the direction parameters to the GPU.
 */
void LightDirection::sendDirection(int i, std::shared_ptr<ShaderProgram> program, const std::string &array) {
	program->setUniformVec(f("{}[{:d}].Direction", array, i), this->getDirection());
}
