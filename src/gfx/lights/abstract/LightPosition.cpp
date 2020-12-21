#include "LightPosition.h"

#include "gfx/gl/program/ShaderProgram.h"
#include "io/Format.h"

using namespace gfx::lights;

/**
 * Sets up a default world-space position.
 */
LightPosition::LightPosition() {
    this->position = glm::vec3(0);
}

/**
 * Sets the position of the light, in world coordinates.
 */
void LightPosition::setPosition(glm::vec3 position) {
    this->position = position;
    this->markDirty();
}

/**
 * Sends the position parameters to the GPU.
 */
void LightPosition::sendPosition(int i, std::shared_ptr<ShaderProgram> program, const std::string &array) {
    program->setUniformVec(f("{}[{:d}].Position", array, i), this->getPosition());
}
