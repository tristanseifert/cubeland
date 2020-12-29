/**
 * Helpers to calculate intersections between rays and various objects.
 */
#ifndef UTIL_INTERSECT_H
#define UTIL_INTERSECT_H

#include <cmath>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace util {
class Intersect {
    public:
        /// Whether the two vectors (of min,max layout) overlap.
        static bool isOverlapping1D(const glm::vec2 &box1, const glm::vec2 &box2) {
            return (box1.y >= box2.x) && (box2.y >= box1.x);
        }

        /**
         * Checks whether there is an intersection by two boxes in 3D space, described by their
         * minimum and maximum coordinates.
         */
        static bool boxBox(const glm::vec3 &lb1, const glm::vec3 &rt1, const glm::vec3 &lb2,
                const glm::vec3 &rt2) {
            return isOverlapping1D(glm::vec2(lb1.x, rt1.x), glm::vec2(lb2.x, rt2.x)) &&
                isOverlapping1D(glm::vec2(lb1.y, rt1.y), glm::vec2(lb2.y, rt2.y)) &&
                isOverlapping1D(glm::vec2(lb1.z, rt1.z), glm::vec2(lb2.z, rt2.z));
        }


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
