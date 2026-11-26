#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <cmath>
#include <glm/glm.hpp>
#include <iostream>

static constexpr float TEST_EPSILON = 1e-4f;

inline bool approxEqual(float a, float b, float epsilon = TEST_EPSILON) {
    return std::fabs(a - b) < epsilon;
}

inline bool approxEqual(double a, double b, double epsilon = 1e-4) {
    return std::fabs(a - b) < epsilon;
}

inline bool vec3ApproxEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = TEST_EPSILON) {
    return approxEqual(a.x, b.x, epsilon) &&
           approxEqual(a.y, b.y, epsilon) &&
           approxEqual(a.z, b.z, epsilon);
}

inline bool vec3Orthogonal(const glm::vec3& a, const glm::vec3& b, float epsilon = TEST_EPSILON) {
    return std::fabs(glm::dot(a, b)) < epsilon;
}

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

#define ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::fabs(static_cast<double>(a) - static_cast<double>(b)) > static_cast<double>(eps)) { \
            std::cerr << "ASSERT_NEAR failed: " #a " (" << (a) << ") vs " #b " (" << (b) \
                      << ") [diff=" << std::fabs(static_cast<double>(a) - static_cast<double>(b)) \
                      << " > " << (eps) << "] at " << __FILE__ << ":" << __LINE__ << std::endl; \
            failures++; \
        } \
    } while(0)

#define ASSERT_VEC3_NEAR(a, b, eps) \
    do { \
        if (!vec3ApproxEqual((a), (b), (eps))) { \
            std::cerr << "ASSERT_VEC3_NEAR failed: " #a " != " #b \
                      << " [(" << (a).x << "," << (a).y << "," << (a).z << ") vs (" \
                      << (b).x << "," << (b).y << "," << (b).z << ")] at " \
                      << __FILE__ << ":" << __LINE__ << std::endl; \
            failures++; \
        } \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "ASSERT_TRUE failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            failures++; \
        } \
    } while(0)

#define ASSERT_FALSE(cond) \
    do { \
        if (cond) { \
            std::cerr << "ASSERT_FALSE failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            failures++; \
        } \
    } while(0)

#endif // TEST_HELPERS_H
