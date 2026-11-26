#include "test_helpers.h"
#include "graphics/camera.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int runCameraTests() {
    int failures = 0;

    // Test 1: Initial camera state and coordinate frame orientation
    {
        Camera cam;
        ASSERT_VEC3_NEAR(cam.position, glm::vec3(0.0f), 1e-4f);
        ASSERT_NEAR(cam.vertical_angle, 0.0, 1e-4);
        ASSERT_NEAR(cam.horizontal_angle, M_PI, 1e-4);

        // Initial orientation: horizontal_angle = PI, vertical_angle = 0
        // direction = (cos(0)*sin(PI), sin(0), cos(0)*cos(PI)) = (0, 0, -1)
        // right = (sin(PI - PI/2), 0, cos(PI - PI/2)) = (1, 0, 0)
        // up = cross(right, direction) = cross((1,0,0), (0,0,-1)) = (0, 1, 0)
        ASSERT_VEC3_NEAR(cam.direction, glm::vec3(0.0f, 0.0f, -1.0f), 1e-4f);
        ASSERT_VEC3_NEAR(cam.right, glm::vec3(1.0f, 0.0f, 0.0f), 1e-4f);
        ASSERT_VEC3_NEAR(cam.up, glm::vec3(0.0f, 1.0f, 0.0f), 1e-4f);
    }

    // Test 2: Pitch clamping upper bound beyond +pi/2
    {
        Camera cam;
        // Aiming beyond +pi/2 (e.g. +2.0, +5.0, +M_PI) must strictly clamp to 1.55
        cam.aim(0.0, M_PI / 2.0 + 0.5);
        ASSERT_NEAR(cam.vertical_angle, 1.55, 1e-5);
        ASSERT_TRUE(cam.vertical_angle <= 1.55);
        ASSERT_TRUE(cam.vertical_angle >= -1.55);

        // Further positive pitch attempts must stay strictly clamped at 1.55
        cam.aim(0.0, 10.0);
        ASSERT_NEAR(cam.vertical_angle, 1.55, 1e-5);
        ASSERT_TRUE(cam.vertical_angle <= 1.55);

        // Verify direction vector stays valid and normalized at maximum pitch
        ASSERT_NEAR(glm::length(cam.direction), 1.0f, 1e-4f);
        ASSERT_NEAR(cam.direction.y, (float)std::sin(1.55), 1e-4f);
    }

    // Test 3: Pitch clamping lower bound beyond -pi/2
    {
        Camera cam;
        // Aiming beyond -pi/2 (e.g. -2.0, -5.0, -M_PI) must strictly clamp to -1.55
        cam.aim(0.0, -(M_PI / 2.0 + 0.5));
        ASSERT_NEAR(cam.vertical_angle, -1.55, 1e-5);
        ASSERT_TRUE(cam.vertical_angle >= -1.55);
        ASSERT_TRUE(cam.vertical_angle <= 1.55);

        // Further negative pitch attempts must stay strictly clamped at -1.55
        cam.aim(0.0, -10.0);
        ASSERT_NEAR(cam.vertical_angle, -1.55, 1e-5);
        ASSERT_TRUE(cam.vertical_angle >= -1.55);

        // Verify direction vector stays valid and normalized at minimum pitch
        ASSERT_NEAR(glm::length(cam.direction), 1.0f, 1e-4f);
        ASSERT_NEAR(cam.direction.y, (float)std::sin(-1.55), 1e-4f);
    }

    // Test 4: Camera orientation synchronization across heading angles
    {
        // Verify forward and backward vectors match heading angles
        Camera cam;

        // Case 4a: Default heading (horizontal_angle = PI) -> forward is -Z, backward is +Z
        {
            glm::vec3 forward = cam.direction;
            glm::vec3 backward = -cam.direction;
            ASSERT_VEC3_NEAR(forward, glm::vec3(0.0f, 0.0f, -1.0f), 1e-4f);
            ASSERT_VEC3_NEAR(backward, glm::vec3(0.0f, 0.0f, 1.0f), 1e-4f);
        }

        // Case 4b: Rotate +90 deg (aim +PI/2 horizontally -> horizontal_angle = 3*PI/2)
        // direction = (sin(3*PI/2), 0, cos(3*PI/2)) = (-1, 0, 0)
        {
            Camera c;
            c.aim(M_PI / 2.0, 0.0);
            glm::vec3 forward = c.direction;
            glm::vec3 backward = -c.direction;
            ASSERT_VEC3_NEAR(forward, glm::vec3(-1.0f, 0.0f, 0.0f), 1e-4f);
            ASSERT_VEC3_NEAR(backward, glm::vec3(1.0f, 0.0f, 0.0f), 1e-4f);
            ASSERT_VEC3_NEAR(c.right, glm::vec3(0.0f, 0.0f, -1.0f), 1e-4f);
        }

        // Case 4c: Rotate -90 deg (aim -PI/2 horizontally -> horizontal_angle = PI/2)
        // direction = (sin(PI/2), 0, cos(PI/2)) = (1, 0, 0)
        {
            Camera c;
            c.aim(-M_PI / 2.0, 0.0);
            glm::vec3 forward = c.direction;
            glm::vec3 backward = -c.direction;
            ASSERT_VEC3_NEAR(forward, glm::vec3(1.0f, 0.0f, 0.0f), 1e-4f);
            ASSERT_VEC3_NEAR(backward, glm::vec3(-1.0f, 0.0f, 0.0f), 1e-4f);
            ASSERT_VEC3_NEAR(c.right, glm::vec3(0.0f, 0.0f, 1.0f), 1e-4f);
        }

        // Case 4d: Rotate 180 deg (aim -PI horizontally -> horizontal_angle = 0)
        // direction = (sin(0), 0, cos(0)) = (0, 0, 1)
        {
            Camera c;
            c.aim(-M_PI, 0.0);
            glm::vec3 forward = c.direction;
            glm::vec3 backward = -c.direction;
            ASSERT_VEC3_NEAR(forward, glm::vec3(0.0f, 0.0f, 1.0f), 1e-4f);
            ASSERT_VEC3_NEAR(backward, glm::vec3(0.0f, 0.0f, -1.0f), 1e-4f);
            ASSERT_VEC3_NEAR(c.right, glm::vec3(-1.0f, 0.0f, 0.0f), 1e-4f);
        }

        // Case 4e: Heading with pitch elevation
        {
            Camera c;
            double pitch = 0.5; // ~28.6 degrees
            c.aim(0.0, pitch);
            // horizontal_angle = PI, vertical_angle = 0.5
            // direction = (cos(0.5)*sin(PI), sin(0.5), cos(0.5)*cos(PI)) = (0, sin(0.5), -cos(0.5))
            glm::vec3 expectedForward(0.0f, (float)std::sin(pitch), (float)-std::cos(pitch));
            ASSERT_VEC3_NEAR(c.direction, expectedForward, 1e-4f);
            ASSERT_VEC3_NEAR(-c.direction, -expectedForward, 1e-4f);
        }
    }

    // Test 5: Forward and backward movement along direction vector
    {
        Camera cam;
        glm::vec3 initialPos = cam.position;
        cam.moveForward(5.0);
        float dist = glm::distance(cam.position, initialPos);
        ASSERT_NEAR(dist, 5.0f, 1e-3f);
        ASSERT_VEC3_NEAR(cam.position, initialPos + cam.direction * 5.0f, 1e-3f);

        cam.moveBackward(5.0);
        ASSERT_VEC3_NEAR(cam.position, initialPos, 1e-3f);

        // Move backward from initial pos
        cam.moveBackward(3.5);
        ASSERT_VEC3_NEAR(cam.position, initialPos - cam.direction * 3.5f, 1e-3f);
        cam.moveForward(3.5);
        ASSERT_VEC3_NEAR(cam.position, initialPos, 1e-3f);
    }

    // Test 6: Lateral strafe movement along orthogonal right vector and forward distance maintenance
    {
        // 6a: Strafing at default orientation
        Camera cam;
        glm::vec3 initialPos = cam.position;

        // Move right
        cam.moveRight(4.0);
        float distRight = glm::distance(cam.position, initialPos);
        ASSERT_NEAR(distRight, 4.0f, 1e-3f);
        // Verify motion is strictly along the right vector
        ASSERT_VEC3_NEAR(cam.position, initialPos + cam.right * 4.0f, 1e-3f);
        // Verify forward distance is maintained (projection onto forward direction is zero)
        ASSERT_NEAR(glm::dot(cam.position - initialPos, cam.direction), 0.0f, 1e-4f);

        // Return to start
        cam.moveLeft(4.0);
        ASSERT_VEC3_NEAR(cam.position, initialPos, 1e-3f);

        // Move left
        cam.moveLeft(2.5);
        float distLeft = glm::distance(cam.position, initialPos);
        ASSERT_NEAR(distLeft, 2.5f, 1e-3f);
        // Verify motion is strictly along -right vector
        ASSERT_VEC3_NEAR(cam.position, initialPos - cam.right * 2.5f, 1e-3f);
        // Verify forward distance is maintained
        ASSERT_NEAR(glm::dot(cam.position - initialPos, cam.direction), 0.0f, 1e-4f);

        // Return to start
        cam.moveRight(2.5);
        ASSERT_VEC3_NEAR(cam.position, initialPos, 1e-3f);

        // 6b: Strafing at arbitrary yaw and pitch
        Camera orientedCam;
        orientedCam.aim(0.785398, 0.4); // 45 deg yaw, ~23 deg pitch
        glm::vec3 startPos = glm::vec3(10.0f, -5.0f, 2.0f);
        orientedCam.position = startPos;

        // Verify right vector is strictly orthogonal to direction
        ASSERT_TRUE(vec3Orthogonal(orientedCam.right, orientedCam.direction, 1e-4f));

        // Strafe right by 6.0 units
        orientedCam.moveRight(6.0);
        glm::vec3 deltaRight = orientedCam.position - startPos;
        ASSERT_VEC3_NEAR(deltaRight, orientedCam.right * 6.0f, 1e-3f);
        // Forward distance must remain unchanged
        ASSERT_NEAR(glm::dot(deltaRight, orientedCam.direction), 0.0f, 1e-4f);
        ASSERT_NEAR(glm::dot(orientedCam.position, orientedCam.direction),
                    glm::dot(startPos, orientedCam.direction), 1e-4f);

        // Strafe left by 10.0 units
        orientedCam.moveLeft(10.0);
        glm::vec3 deltaLeft = orientedCam.position - startPos;
        ASSERT_VEC3_NEAR(deltaLeft, orientedCam.right * -4.0f, 1e-3f);
        // Forward distance must still remain unchanged
        ASSERT_NEAR(glm::dot(deltaLeft, orientedCam.direction), 0.0f, 1e-4f);
        ASSERT_NEAR(glm::dot(orientedCam.position, orientedCam.direction),
                    glm::dot(startPos, orientedCam.direction), 1e-4f);
    }

    // Test 7: Orthonormal basis verification across diverse angles
    {
        const double angles[][2] = {
            {0.0, 0.0},
            {0.5, 0.3},
            {-1.2, -0.8},
            {2.5, 1.5},
            {-2.8, -1.5},
            {M_PI, 0.0}
        };

        for (const auto& a : angles) {
            Camera c;
            c.aim(a[0], a[1]);

            // Direction, right, and up must be unit vectors
            ASSERT_NEAR(glm::length(c.direction), 1.0f, 1e-4f);
            ASSERT_NEAR(glm::length(c.right), 1.0f, 1e-4f);
            ASSERT_NEAR(glm::length(c.up), 1.0f, 1e-4f);

            // They must be mutually orthogonal
            ASSERT_TRUE(vec3Orthogonal(c.direction, c.right, 1e-4f));
            ASSERT_TRUE(vec3Orthogonal(c.direction, c.up, 1e-4f));
            ASSERT_TRUE(vec3Orthogonal(c.right, c.up, 1e-4f));
        }
    }

    // Test 8: Camera FOV Projection Updates, Clamping & Aspect Preservation
    {
        Camera cam;
        ASSERT_NEAR(cam.fov_degrees, 45.0f, 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[1][1],
                    1.0f / std::tan(glm::radians(45.0f) * 0.5f), 1e-4f);

        cam.setFov(75.0f);
        ASSERT_NEAR(cam.fov_degrees, 75.0f, 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[1][1],
                    1.0f / std::tan(glm::radians(75.0f) * 0.5f), 1e-4f);

        cam.setFov(25.0f);
        ASSERT_NEAR(cam.fov_degrees, 25.0f, 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[1][1],
                    1.0f / std::tan(glm::radians(25.0f) * 0.5f), 1e-4f);

        // Verify out-of-range clamping [10.0f, 120.0f]
        cam.setFov(5.0f);
        ASSERT_NEAR(cam.fov_degrees, 10.0f, 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[1][1],
                    1.0f / std::tan(glm::radians(10.0f) * 0.5f), 1e-4f);

        cam.setFov(160.0f);
        ASSERT_NEAR(cam.fov_degrees, 120.0f, 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[1][1],
                    1.0f / std::tan(glm::radians(120.0f) * 0.5f), 1e-4f);

        // Verify setProjection updates both FOV and aspect ratio
        const float aspect_16_9 = 16.0f / 9.0f;
        cam.setProjection(65.0f, aspect_16_9);
        ASSERT_NEAR(cam.fov_degrees, 65.0f, 1e-4f);
        ASSERT_NEAR(cam.aspect_ratio, aspect_16_9, 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[1][1],
                    1.0f / std::tan(glm::radians(65.0f) * 0.5f), 1e-4f);
        ASSERT_NEAR(cam.projection_matrix[0][0],
                    cam.projection_matrix[1][1] / aspect_16_9, 1e-4f);
    }

    return failures;
}
