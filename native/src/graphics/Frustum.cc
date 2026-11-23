#include "graphics/Frustum.h"
#include <cmath>

void Frustum::update(const glm::mat4& m) {
    // Left: col3 + col0
    planes[LEFT] = glm::vec4(
        m[0][3] + m[0][0],
        m[1][3] + m[1][0],
        m[2][3] + m[2][0],
        m[3][3] + m[3][0]
    );

    // Right: col3 - col0
    planes[RIGHT] = glm::vec4(
        m[0][3] - m[0][0],
        m[1][3] - m[1][0],
        m[2][3] - m[2][0],
        m[3][3] - m[3][0]
    );

    // Bottom: col3 + col1
    planes[BOTTOM] = glm::vec4(
        m[0][3] + m[0][1],
        m[1][3] + m[1][1],
        m[2][3] + m[2][1],
        m[3][3] + m[3][1]
    );

    // Top: col3 - col1
    planes[TOP] = glm::vec4(
        m[0][3] - m[0][1],
        m[1][3] - m[1][1],
        m[2][3] - m[2][1],
        m[3][3] - m[3][1]
    );

    // Near: col3 + col2
    planes[NEAR_PLANE] = glm::vec4(
        m[0][3] + m[0][2],
        m[1][3] + m[1][2],
        m[2][3] + m[2][2],
        m[3][3] + m[3][2]
    );

    // Far: col3 - col2
    planes[FAR_PLANE] = glm::vec4(
        m[0][3] - m[0][2],
        m[1][3] - m[1][2],
        m[2][3] - m[2][2],
        m[3][3] - m[3][2]
    );

    // Normalize each plane equation (A*x + B*y + C*z + D = 0)
    for (auto& plane : planes) {
        float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        if (length > 1e-8f) {
            plane /= length;
        }
    }
}

bool Frustum::isSphereInside(const glm::vec3& center, float radius) const {
    if (radius < 0.0f) {
        return false;
    }
    for (const auto& plane : planes) {
        float distance = plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}
