#include "test_helpers.h"
#include "graphics/Frustum.h"
#include <glm/gtc/matrix_transform.hpp>

int runFrustumTests() {
    int failures = 0;

    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.333f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 vp = proj * view;

    Frustum frustum;
    frustum.update(vp);

    // Test 1: Sphere at origin (inside view frustum)
    {
        if (!frustum.isSphereInside(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f)) {
            std::cerr << "FrustumTest: Center sphere reported outside" << std::endl;
            failures++;
        }
    }

    // Test 2: Sphere behind camera
    {
        if (frustum.isSphereInside(glm::vec3(0.0f, 0.0f, 20.0f), 1.0f)) {
            std::cerr << "FrustumTest: Behind-camera sphere reported inside" << std::endl;
            failures++;
        }
    }

    // Test 3: Sphere far beyond far clipping plane
    {
        if (frustum.isSphereInside(glm::vec3(0.0f, 0.0f, -200.0f), 1.0f)) {
            std::cerr << "FrustumTest: Far-plane sphere reported inside" << std::endl;
            failures++;
        }
    }

    // Test 4: Sphere far to the right
    {
        if (frustum.isSphereInside(glm::vec3(150.0f, 0.0f, 0.0f), 2.0f)) {
            std::cerr << "FrustumTest: Far-right sphere reported inside" << std::endl;
            failures++;
        }
    }

    // Test 5: Negative radius safety
    {
        if (frustum.isSphereInside(glm::vec3(0.0f, 0.0f, 0.0f), -1.0f)) {
            std::cerr << "FrustumTest: Negative radius should report outside" << std::endl;
            failures++;
        }
    }

    return failures;
}
