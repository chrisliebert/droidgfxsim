#ifndef _MATH_UTIL_H_
#define _MATH_UTIL_H_

#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

namespace Math {
    struct Sphere {
        glm::vec3 center{0.0f};
        float radius{0.0f};
    };

    Sphere calculateBoundingSphere(const glm::vec3* points, size_t numPoints);
    Sphere transformBoundingSphere(const glm::mat4& transform, const glm::vec3& center, float radius);
}

#endif // _MATH_UTIL_H_
