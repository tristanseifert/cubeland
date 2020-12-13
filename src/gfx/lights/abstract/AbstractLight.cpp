/*
 * AbstractLight.cpp
 *
 *  Created on: Sep 1, 2015
 *      Author: tristan
 */

#include "AbstractLight.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "io/Format.h"

using namespace std;
using namespace gfx::lights;

/**
 * Sets up some default light properties.
 */
AbstractLight::AbstractLight() {
	this->diffuseColor = glm::vec3(0, 1, 0);
	this->specularColor = glm::vec3(1, 0, 1);
}

/**
 * Clean up whatever.
 */
AbstractLight::~AbstractLight() {

}

/**
 * Sets the colour used for the diffuse highlights created by this light.
 */
void AbstractLight::setDiffuseColor(glm::vec3 diffuse) {
	this->diffuseColor = diffuse;
}
/**
 * Sets the colour used for the specular highlights created by this light.
 */
void AbstractLight::setSpecularColor(glm::vec3 specular) {
	this->specularColor = specular;
}

/**
 * Sends the light's colours.
 */
void AbstractLight::sendColors(int i, std::shared_ptr<ShaderProgram> program,
                                const std::string &array) {
    program->setUniformVec(f("{}[{:d}].SpecularColour", array, i), this->getSpecularColor());
    program->setUniformVec(f("{}[{:d}].DiffuseColour", array, i), this->getDiffuseColor());
}
