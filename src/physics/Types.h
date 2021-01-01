/**
 * Provides some helpers to convert between the physics engine and existing vector types.
 */
#ifndef PHYSICS_TYPES_H
#define PHYSICS_TYPES_H

#include <reactphysics3d/mathematics/Vector2.h>
#include <reactphysics3d/mathematics/Vector3.h>
#include <reactphysics3d/mathematics/Quaternion.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace physics {
/// Converts a physics engine 2 component vector to glm
static inline const glm::vec2 vec(const reactphysics3d::Vector2 &in) {
    return glm::vec2(in.x, in.y);
}
/// Converts a glm 2 component vector to physics engine
static inline const reactphysics3d::Vector2 vec(const glm::vec2 &in) {
    return reactphysics3d::Vector2(in.x, in.y);
}

/// Converts a physics engine 3 component vector to glm
static inline const glm::vec3 vec(const reactphysics3d::Vector3 &in) {
    return glm::vec3(in.x, in.y, in.z);
}
/// Converts a glm 3 component vector to physics engine
static inline const reactphysics3d::Vector3 vec(const glm::vec3 &in) {
    return reactphysics3d::Vector3(in.x, in.y, in.z);
}
static inline const reactphysics3d::Vector3 vec(const glm::ivec3 &in) {
    return reactphysics3d::Vector3(in.x, in.y, in.z);
}

// converts a quaternion
static inline const glm::quat vec(const reactphysics3d::Quaternion &in) {
    return glm::quat(in.w, in.x, in.y, in.z);
}
}

#endif
