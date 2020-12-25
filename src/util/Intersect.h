/**
 * Helpers to calculate intersections between rays and various objects.
 */
#ifndef UTIL_INTERSECT_H
#define UTIL_INTERSECT_H

#include <cmath>

#include <glm/vec3.hpp>

namespace util {
class Intersect {
    public:
        /**
         * Checks whether the ray starting at `origin` with the inverse direction `dirfrac`
         * intersects the rectangular region described by minimum coordinate `lb` and maximum `rt`.
         *
         * `dirfrac` is calculated as 1 / direction. Precalculate this if testing the same ray
         * against many different meepers.
         */
        static bool rayArbb(const glm::vec3 &origin, const glm::vec3 &dirfrac, const glm::vec3 &lb,
                const glm::vec3 &rt) {
            float t1 = (lb.x - origin.x)*dirfrac.x;
            float t2 = (rt.x - origin.x)*dirfrac.x;
            float t3 = (lb.y - origin.y)*dirfrac.y;
            float t4 = (rt.y - origin.y)*dirfrac.y;
            float t5 = (lb.z - origin.z)*dirfrac.z;
            float t6 = (rt.z - origin.z)*dirfrac.z;

            float tmin = fmax(fmax(fmin(t1, t2), fmin(t3, t4)), fmin(t5, t6));
            float tmax = fmin(fmin(fmax(t1, t2), fmax(t3, t4)), fmax(t5, t6));
            float t = 0;

            // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
            if (tmax < 0) {
                t = tmax;
                return false;
            }

            // if tmin > tmax, ray doesn't intersect AABB
            if (tmin > tmax) {
                t = tmax;
                return false;
            }

            t = tmin;
            return true;
        }
};
}

#endif
