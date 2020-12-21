#include "PointLight.h"

using namespace gfx;

/**
 * Sets the default light type.
 */
PointLight::PointLight() {
    this->type = lights::AbstractLight::Point;
}

/**
 * Sends the uniforms required to render this light to the lighting shader.
 *
 * @note Assumes the standard light structures, as defined in `lighting.shader.`
 */
void PointLight::sendToProgram(const int i, std::shared_ptr<ShaderProgram> program) {
    this->sendPosition(i, program, "pointLights");
    this->sendColors(i, program, "pointLights");
    this->sendAttenuation(i, program, "pointLights");

    this->dirty = false;
}
