#include "System.h"

#include "physics/Engine.h"
#include "physics/Types.h"

#include "io/Format.h"
#include <Logging.h>

#include <algorithm>

#include <glm/glm.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#include <reactphysics3d/reactphysics3d.h> 
#pragma GCC diagnostic pop

using namespace particles;

/**
 * Prepares the initial state of the particle system.
 */
System::System(const glm::vec3 &_origin) : origin(_origin) {
    
}

/**
 * Cleans up physics resources left on the particle system.
 */
System::~System() {
    // erase all particles
    for(auto &particle : this->particles) {
        this->prepareParticleForDealloc(particle);
    }
    this->particles.clear();

    // and the physics resources
    auto sphere = dynamic_cast<reactphysics3d::SphereShape *>(this->collideShape);
    XASSERT(!(!sphere && this->collideShape), "Unknown collision shape");

    if(sphere) {
        this->physics->getCommon()->destroySphereShape(sphere);
    }
}

/**
 * Sets the physics engine pointer, and allocates some required physics resources.
 */
void System::setPhysicsEngine(physics::Engine *_physics) {
    this->collideShape = _physics->getCommon()->createSphereShape(this->particleRadius);

    this->physics = _physics;
}

/**
 * Gets the bounding box describing this particle system, e.g. the area in which particles may be
 * located.
 *
 * It's not super critical that this is correct; it should err on the side of encompassing more
 * space than actually needed, if this benefits speed. It's only used to cull away particle systems
 * that are totally off screen.
 */
void System::getBounds(glm::vec3 &lb, glm::vec3 &rt) {
    const float kRadius = 1., kRadiusY = 4.;

    lb = glm::vec3(origin.x - kRadius / 2., origin.y, origin.z - kRadius / 2.);
    rt = glm::vec3(origin.x + kRadius / 2., origin.y + kRadiusY, origin.z + kRadius / 2.);
}

/**
 * Performs the aging step of the particle system, where new particles are created, and old ones
 * die.
 */
void System::agingStep(const bool canSpawn) {
    // go through all particles and add one to their age
    for(auto &particle : this->particles) {
        particle.age++;
    }

    // age out old particles
    std::erase_if(this->particles, [&](auto &particle) {
        if(particle.age >= particle.maxAge) {
            this->prepareParticleForDealloc(particle);
            return true;
        }

        return false;
    });

    // should we generate a particle?
    if(canSpawn) {
        std::uniform_real_distribution<float> genDist(0., 1.);

        for(size_t i = 0; i < this->spawnRounds; i++) {
            if(this->particles.size() < this->maxParticles && 
               genDist(this->randGen) <= this->spawnProbability) {
                this->allocNewParticle();
            }
        }
    }
}

/**
 * Instantiates a new particle.
 */
void System::allocNewParticle() {
    using namespace reactphysics3d;
    auto pw = this->physics->getWorld();

    // decide initial conditions for particle
    Particle p;

    std::uniform_int_distribution<size_t> ageDist(this->minParticleAge, this->maxParticleAge);
    p.maxAge = ageDist(this->randGen);

    // create rigid body and set up its collider
    Transform t(physics::vec(this->origin), Quaternion::identity());
    auto bod = pw->createRigidBody(t);

    std::uniform_real_distribution<float> massDist(this->mass.x, this->mass.y);
    bod->setMass(massDist(this->randGen));
    bod->setLinearDamping(this->linearDamping);
    bod->enableGravity(false);

    p.physBody = bod;

    auto col = bod->addCollider(this->collideShape, Transform::identity());
    col->setCollisionCategoryBits(physics::Engine::kParticles);
    col->setCollideWithMaskBits(0);
    p.physCol = col;

    // apply its initial force
    auto force = this->initialForce;

    for(size_t i = 0; i < 3; i++) {
        const auto var = this->forceVariation[i];
        if(var == 0) continue; // check for EXACTLY zero

        std::uniform_real_distribution<float> fact(-var, var);
        force[i] += fact(this->randGen);
    }

    bod->applyForceToCenterOfMass(physics::vec(force));

    // spawn particle
    this->particles.push_back(p);
}

/**
 * Removes physics bodies associated with a particle.
 */
void System::prepareParticleForDealloc(Particle &p) {
    auto w = this->physics->getWorld();

    // remove body if it was allocated
    if(p.physBody) {
        w->destroyRigidBody(p.physBody);
    }
}

/**
 * For each visible particle, build a particle info struct.
 */
void System::buildParticleBuf(std::vector<Renderer::ParticleInfo> &particles) {
    for(const auto &particle : this->particles) {
        // calculate alpha; such that it fades out the last deathLength frames of its existence
        float alpha = 1.;

        if(particle.age >= (particle.maxAge - this->deathLength)) {
            alpha = (((float) particle.maxAge - particle.age) / ((float) this->deathLength));
        }

        // insert the info
        particles.emplace_back(Renderer::ParticleInfo({
            .pos = particle.getPosition(),
            .color = this->tintForParticle(particle),
            .uv = this->uvForParticle(particle),
            .scale = ((this->particleRadius * 2) / 1.f),
            .alpha = alpha
        }));
    }
}




/**
 * Gets the position of the particle's physics body.
 */
const glm::vec3 System::Particle::getPosition() const {
    return physics::vec(this->physBody->getTransform().getPosition());
}



/**
 * Registers the default particle texture.
 */
void System::registerTextures(Renderer *rend) {
    if(!rend->addTexture(glm::vec2(32, 32), "particle/default.png")) {
        this->textureAtlasUpdated(rend);
    }
}

/**
 * Gets the UV of the default texture and saves it as the cached UV. value.
 */
void System::textureAtlasUpdated(Renderer *rend) {
    this->defaultUv = rend->getUv("particle/default.png");
}
