#include "Block.h"

#include "particles/Renderer.h"
#include "particles/System.h"
#include "render/scene/SceneRenderer.h"
#include "render/steps/Lighting.h"
#include "inventory/Manager.h"

#include <memory>

using namespace world;

/// pointer to the global particle renderer
extern std::shared_ptr<particles::Renderer> gParticleRenderer;
/// pointer to the global lighting renderer
extern std::shared_ptr<render::Lighting> gLightRenderer;
/// pointer to the global inventory manager
extern inventory::Manager *gInventoryManager;


/**
 * Adds the given particle system to the particle renderer for the currently active scene.
 */
void Block::addParticleSystem(std::shared_ptr<particles::System> sys) {
    if(!gParticleRenderer) return;
    gParticleRenderer->addSystem(sys);
}

/**
 * Removes a previously added particle system.
 */
void Block::removeParticleSystem(std::shared_ptr<particles::System> sys) {
    if(!gParticleRenderer) return;
    gParticleRenderer->removeSystem(sys);
}



/**
 * Adds a new light.
 */
void Block::addLight(std::shared_ptr<gfx::lights::AbstractLight> light) {
    if(!gLightRenderer) return;
    gLightRenderer->addLight(light);
}

/**
 * Removes a previously added light.
 */
void Block::removeLight(std::shared_ptr<gfx::lights::AbstractLight> light) {
    if(!gLightRenderer) return;
    gLightRenderer->removeLight(light);
}



/**
 * Adds an item to the inventory.
 *
 * @return Whether the item was successfully added to the inventory, e.g. whether the player has
 * inventory space available.
 */
bool Block::addInventoryItem(const uuids::uuid &id, const size_t count) {
    if(!gInventoryManager) return false;
    return gInventoryManager->addItem(id, count);
}
