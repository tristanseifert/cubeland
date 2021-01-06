/**
 * Implements a basic particle system, which serves as the source for some particles that get
 * secreted out into the world.
 */
#ifndef PARTICLES_SYSTEM_H
#define PARTICLES_SYSTEM_H

#include "Renderer.h"

#include <chrono>
#include <vector>
#include <random>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace reactphysics3d {
class CollisionShape;
class RigidBody;
class Collider;
}

namespace physics {
class Engine;
}

namespace particles {

class System {
    friend class Renderer;

    public:
        System(const glm::vec3 &origin);
        virtual ~System();

        virtual void getBounds(glm::vec3 &lb, glm::vec3 &rt);
        virtual void agingStep(const bool canSpawn = true);

        /// Called when the particle system is first added to register its textures
        virtual void registerTextures(Renderer *rend);
        /// Invoked when the texture atlas is updated. Cached UVs should be updated.
        virtual void textureAtlasUpdated(Renderer *rend);

    protected:
        struct Particle {
            friend class System;

            public:
                /// Age of particle, in ms
                size_t age = 0;
                /// when this particle shall die (age wise)
                size_t maxAge = 0;

                /// tint for the particle
                glm::vec3 tint = glm::vec3(1.);

            public:
                const glm::vec3 getPosition() const;

            private:
                /// rigid body for the particle
                reactphysics3d::RigidBody *physBody = nullptr;
                /// its associated collider
                reactphysics3d::Collider *physCol = nullptr;
        };

    protected:
        /// Returns the UV coordinates in the particle engine texture map.
        virtual glm::vec4 uvForParticle(const Particle &) {
            return this->defaultUv;
        }

    private:
        void setPhysicsEngine(physics::Engine *phys);

        virtual void allocNewParticle();
        virtual void prepareParticleForDealloc(Particle &p);

        virtual void buildParticleBuf(std::vector<Renderer::ParticleInfo> &particles);

    protected:
        glm::vec3 origin;

        /// all particles of this system
        std::vector<Particle> particles;

        /// radius of particles (in m)
        float particleRadius = .05;

        /// particle mass in kg (lower bound, upper bound)
        glm::vec2 mass = glm::vec2(0.00005, 0.0001); // 50mg - 100mg
        /// linear damping factor
        float linearDamping = 0.33;

        /// maximum "rounds" of spawning per frame
        size_t spawnRounds = 3;
        /// probability that a particle is spawned in any given frame
        float spawnProbability = 0.74;
        /// initial force to give the particle
        glm::vec3 initialForce = glm::vec3(0, .0085, 0);
        /// +/- force variation (randomly generated)
        glm::vec3 forceVariation = glm::vec3(0.00285, 0.0002, 0.00285);

        /// maximum number of particles
        size_t maxParticles = 250;

        /// length of a particle's death (e.g. when it fades out)
        size_t deathLength = 15;
        /// maximum age, in frames, of a particle
        size_t maxParticleAge = 150;
        /// minimum age of a particle, in frames, before elimination
        size_t minParticleAge = 60;

    private:
        /// random generator
        std::mt19937 randGen = std::mt19937(std::random_device()());

        /// pointer to the global physics engine, for simulating particles
        physics::Engine *physics = nullptr;
        /// collision shape body for particles
        reactphysics3d::CollisionShape *collideShape = nullptr;

        /// UV of the default particle texture
        glm::vec4 defaultUv = glm::vec4(0, 0, 1, 1);
};
}

#endif
