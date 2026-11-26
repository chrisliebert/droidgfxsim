#include "common/MathUtil.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <algorithm>
#include <random>
#include <cmath>
#include <vector>

namespace Math {

namespace {

inline bool contains(const Sphere& s, const glm::vec3& p) {
    const float distSq = glm::dot(p - s.center, p - s.center);
    const float tol = std::max(1e-5f, s.radius * 1e-5f);
    const float rTol = s.radius + tol;
    return distSq <= rTol * rTol;
}

inline Sphere sphereFrom0() {
    return Sphere{glm::vec3(0.0f), 0.0f};
}

inline Sphere sphereFrom1(const glm::vec3& p) {
    return Sphere{p, 0.0f};
}

inline Sphere sphereFrom2(const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 center = (a + b) * 0.5f;
    const float radius = glm::distance(a, b) * 0.5f;
    return Sphere{center, radius};
}

Sphere sphereFrom3(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    const glm::vec3 v1 = a - c;
    const glm::vec3 v2 = b - c;
    const glm::vec3 n = glm::cross(v1, v2);
    const float nLenSq = glm::dot(n, n);

    if (nLenSq < 1e-12f) {
        const float d_ab = glm::dot(a - b, a - b);
        const float d_bc = glm::dot(b - c, b - c);
        const float d_ca = glm::dot(c - a, c - a);
        if (d_ab >= d_bc && d_ab >= d_ca) return sphereFrom2(a, b);
        if (d_bc >= d_ab && d_bc >= d_ca) return sphereFrom2(b, c);
        return sphereFrom2(c, a);
    }

    const glm::vec3 offset = glm::cross(glm::dot(v1, v1) * v2 - glm::dot(v2, v2) * v1, n) / (2.0f * nLenSq);
    const glm::vec3 center = c + offset;
    const float radius = glm::length(offset);
    Sphere s{center, radius};

    const Sphere s_ab = sphereFrom2(a, b);
    if (s_ab.radius < s.radius && contains(s_ab, c)) s = s_ab;
    const Sphere s_bc = sphereFrom2(b, c);
    if (s_bc.radius < s.radius && contains(s_bc, a)) s = s_bc;
    const Sphere s_ca = sphereFrom2(c, a);
    if (s_ca.radius < s.radius && contains(s_ca, b)) s = s_ca;

    return s;
}

Sphere sphereFrom4(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4) {
    const glm::vec3 v1 = p1 - p4;
    const glm::vec3 v2 = p2 - p4;
    const glm::vec3 v3 = p3 - p4;

    const glm::vec3 c23 = glm::cross(v2, v3);
    const glm::vec3 c31 = glm::cross(v3, v1);
    const glm::vec3 c12 = glm::cross(v1, v2);

    const float det = glm::dot(v1, c23);
    Sphere bestSphere{glm::vec3(0.0f), std::numeric_limits<float>::infinity()};

    if (std::abs(det) >= 1e-10f) {
        const glm::vec3 offset = (glm::dot(v1, v1) * c23 +
                                  glm::dot(v2, v2) * c31 +
                                  glm::dot(v3, v3) * c12) / (2.0f * det);
        if (!std::isnan(offset.x) && !std::isnan(offset.y) && !std::isnan(offset.z) &&
            !std::isinf(offset.x) && !std::isinf(offset.y) && !std::isinf(offset.z)) {
            bestSphere = Sphere{p4 + offset, glm::length(offset)};
        }
    }

    const glm::vec3* pts[4] = { &p1, &p2, &p3, &p4 };
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            for (int k = j + 1; k < 4; ++k) {
                Sphere s = sphereFrom3(*pts[i], *pts[j], *pts[k]);
                if (s.radius < bestSphere.radius &&
                    contains(s, p1) && contains(s, p2) &&
                    contains(s, p3) && contains(s, p4)) {
                    bestSphere = s;
                }
            }
        }
    }

    if (std::isinf(bestSphere.radius)) {
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                Sphere s = sphereFrom2(*pts[i], *pts[j]);
                if (s.radius < bestSphere.radius &&
                    contains(s, p1) && contains(s, p2) &&
                    contains(s, p3) && contains(s, p4)) {
                    bestSphere = s;
                }
            }
        }
    }

    return bestSphere;
}

Sphere b_minidisk(std::vector<glm::vec3>& P, size_t n, std::vector<glm::vec3>& R) {
    if (n == 0 || R.size() == 4) {
        switch (R.size()) {
            case 0: return sphereFrom0();
            case 1: return sphereFrom1(R[0]);
            case 2: return sphereFrom2(R[0], R[1]);
            case 3: return sphereFrom3(R[0], R[1], R[2]);
            case 4: return sphereFrom4(R[0], R[1], R[2], R[3]);
        }
    }

    const glm::vec3 p = P[n - 1];
    Sphere d = b_minidisk(P, n - 1, R);

    if (contains(d, p)) {
        return d;
    }

    R.push_back(p);
    Sphere res = b_minidisk(P, n - 1, R);
    R.pop_back();
    return res;
}

} // anonymous namespace

Sphere calculateBoundingSphere(const glm::vec3* points, size_t numPoints) {
    if (numPoints == 0 || points == nullptr) {
        return Sphere{glm::vec3(0.0f), 0.0f};
    }
    if (numPoints == 1) {
        return Sphere{points[0], 0.0f};
    }
    if (numPoints == 2) {
        return sphereFrom2(points[0], points[1]);
    }

    std::vector<glm::vec3> P(points, points + numPoints);
    std::mt19937 g(1337);
    std::shuffle(P.begin(), P.end(), g);

    std::vector<glm::vec3> R;
    R.reserve(4);

    return b_minidisk(P, P.size(), R);
}

Sphere transformBoundingSphere(const glm::mat4& transform, const glm::vec3& center, float radius) {
    const glm::vec4 transformedCenter = transform * glm::vec4(center, 1.0f);
    const float sx = glm::length(glm::vec3(transform[0]));
    const float sy = glm::length(glm::vec3(transform[1]));
    const float sz = glm::length(glm::vec3(transform[2]));
    const float maxScale = std::max({sx, sy, sz});

    return Sphere{
        glm::vec3(transformedCenter),
        radius * maxScale
    };
}

} // namespace Math
