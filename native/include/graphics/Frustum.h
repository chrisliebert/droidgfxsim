#ifndef _FRUSTUM_H_
#define _FRUSTUM_H_

#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <array>

class Frustum {
public:
    enum PlaneIndex {
        LEFT = 0,
        RIGHT,
        BOTTOM,
        TOP,
        NEAR_PLANE,
        FAR_PLANE,
        COUNT
    };

    Frustum() = default;

    // Extract frustum planes using Gribb & Hartmann method from ViewProjection matrix
    void update(const glm::mat4& viewProj);

    // Test whether a sphere is inside or intersects the frustum
    bool isSphereInside(const glm::vec3& center, float radius) const;

    const std::array<glm::vec4, COUNT>& getPlanes() const { return planes; }

private:
    std::array<glm::vec4, COUNT> planes{};
};

#endif // _FRUSTUM_H_
