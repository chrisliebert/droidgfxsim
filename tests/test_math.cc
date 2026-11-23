#include "test_helpers.h"
#include "common/MathUtil.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

int runMathTests() {
    int failures = 0;

    // Test 1: Empty set
    {
        Math::Sphere s = Math::calculateBoundingSphere(nullptr, 0);
        if (!approxEqual(s.radius, 0.0f) || !vec3ApproxEqual(s.center, glm::vec3(0.0f))) {
            std::cerr << "MathTest: Empty set failed" << std::endl;
            failures++;
        }
    }

    // Test 2: Single point
    {
        glm::vec3 p(1.0f, 2.0f, 3.0f);
        Math::Sphere s = Math::calculateBoundingSphere(&p, 1);
        if (!approxEqual(s.radius, 0.0f) || !vec3ApproxEqual(s.center, p)) {
            std::cerr << "MathTest: Single point failed" << std::endl;
            failures++;
        }
    }

    // Test 3: Two points
    {
        glm::vec3 pts[2] = { glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f) };
        Math::Sphere s = Math::calculateBoundingSphere(pts, 2);
        if (!approxEqual(s.radius, 2.0f) || !vec3ApproxEqual(s.center, glm::vec3(0.0f))) {
            std::cerr << "MathTest: Two points failed" << std::endl;
            failures++;
        }
    }

    // Test 4: Equilateral triangle in XY plane
    {
        glm::vec3 pts[3] = {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(-0.866025f, -0.5f, 0.0f),
            glm::vec3(0.866025f, -0.5f, 0.0f)
        };
        Math::Sphere s = Math::calculateBoundingSphere(pts, 3);
        if (!approxEqual(s.radius, 1.0f, 1e-3f) || !vec3ApproxEqual(s.center, glm::vec3(0.0f), 1e-3f)) {
            std::cerr << "MathTest: Triangle circumsphere failed" << std::endl;
            failures++;
        }
    }

    // Test 5: Regular tetrahedron
    {
        glm::vec3 pts[4] = {
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, 1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f)
        };
        Math::Sphere s = Math::calculateBoundingSphere(pts, 4);
        float expectedRadius = std::sqrt(3.0f);
        if (!approxEqual(s.radius, expectedRadius, 1e-3f) || !vec3ApproxEqual(s.center, glm::vec3(0.0f), 1e-3f)) {
            std::cerr << "MathTest: Tetrahedron circumsphere failed" << std::endl;
            failures++;
        }
    }

    // Test 6: Unit cube enclosing sphere
    {
        std::vector<glm::vec3> cube;
        for (float x : {-1.0f, 1.0f}) {
            for (float y : {-1.0f, 1.0f}) {
                for (float z : {-1.0f, 1.0f}) {
                    cube.emplace_back(x, y, z);
                }
            }
        }
        Math::Sphere s = Math::calculateBoundingSphere(cube.data(), cube.size());
        float expectedRadius = std::sqrt(3.0f);
        if (!approxEqual(s.radius, expectedRadius, 1e-3f) || !vec3ApproxEqual(s.center, glm::vec3(0.0f), 1e-3f)) {
            std::cerr << "MathTest: Cube enclosing sphere failed" << std::endl;
            failures++;
        }
    }

    // Test 7: Transform bounding sphere
    {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, -2.0f, 10.0f));
        t = glm::scale(t, glm::vec3(2.0f));
        Math::Sphere s = Math::transformBoundingSphere(t, glm::vec3(0.0f), 3.0f);
        if (!approxEqual(s.radius, 6.0f) || !vec3ApproxEqual(s.center, glm::vec3(5.0f, -2.0f, 10.0f))) {
            std::cerr << "MathTest: Transform bounding sphere failed" << std::endl;
            failures++;
        }
    }

    return failures;
}
