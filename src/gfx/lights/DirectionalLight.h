/*
 * A directional light, which models many parallel light rays—such as the sun.
 */

#ifndef GFX_LEVEL_PRIMITIVES_LIGHTS_DIRECTIONALLIGHT_H_
#define GFX_LEVEL_PRIMITIVES_LIGHTS_DIRECTIONALLIGHT_H_

#include <memory>

#include "abstract/LightDirection.h"
#include "abstract/AbstractLight.h"

namespace gfx {
class DirectionalLight: public lights::LightDirection,
        public lights::AbstractLight {
    public:
        DirectionalLight();
        virtual ~DirectionalLight() = default;

        void sendToProgram(const int index, std::shared_ptr<ShaderProgram> program);

    protected:
        virtual void markDirty() {
            this->dirty = true;
        }
};
} /* namespace gfx */

#endif /* GFX_LEVEL_PRIMITIVES_LIGHTS_DIRECTIONALLIGHT_H_ */
