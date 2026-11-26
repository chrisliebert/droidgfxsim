// Copyright (C) 2017-2026 Chris Liebert
// Unit tests for RaycastPicker unprojection and Bullet dynamic constraints

#include "test_helpers.h"
#include "physics/raycast_picker.h"
#include "physics/simulation.h"
#include "graphics/camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>
#include <vector>

using namespace droidgfxsim;

// Test simulation harness to expose protected addPhysicsNode / addPhysicsBoxNode methods
class RaycastTestSimulation : public Simulation {
public:
    using Simulation::addPhysicsNode;
    using Simulation::addPhysicsBoxNode;
};

int runRaycastTests() {
    int failures = 0;

    // =========================================================================
    // Test 1: Mathematical Unprojection with Orthographic Projection
    // =========================================================================
    {
        const int width = 800;
        const int height = 600;
        const float ortho_size = 10.0f;
        const float near_plane = 1.0f;
        const float far_plane = 100.0f;

        // Identity view: camera at origin looking down -Z
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);
        glm::mat4 inv_vp = glm::inverse(proj * view);

        // Center: (400, 300) -> NDC (0, 0)
        Ray center_ray = RaycastPicker::unproject(400.0f, 300.0f, width, height, inv_vp);
        ASSERT_NEAR(center_ray.origin.x, 0.0f, 1e-4f);
        ASSERT_NEAR(center_ray.origin.y, 0.0f, 1e-4f);
        ASSERT_NEAR(center_ray.origin.z, -near_plane, 1e-4f);
        ASSERT_NEAR(center_ray.direction.x, 0.0f, 1e-4f);
        ASSERT_NEAR(center_ray.direction.y, 0.0f, 1e-4f);
        ASSERT_NEAR(center_ray.direction.z, -1.0f, 1e-4f);
        ASSERT_NEAR(glm::length(center_ray.direction), 1.0f, 1e-5f);

        // Top-Left: (0, 0) -> NDC (-1, 1)
        Ray tl_ray = RaycastPicker::unproject(0.0f, 0.0f, width, height, inv_vp);
        ASSERT_NEAR(tl_ray.origin.x, -ortho_size, 1e-4f);
        ASSERT_NEAR(tl_ray.origin.y, ortho_size, 1e-4f);
        ASSERT_NEAR(tl_ray.origin.z, -near_plane, 1e-4f);
        ASSERT_NEAR(glm::length(tl_ray.direction), 1.0f, 1e-5f);

        // Bottom-Right: (800, 600) -> NDC (1, -1)
        Ray br_ray = RaycastPicker::unproject(800.0f, 600.0f, width, height, inv_vp);
        ASSERT_NEAR(br_ray.origin.x, ortho_size, 1e-4f);
        ASSERT_NEAR(br_ray.origin.y, -ortho_size, 1e-4f);
        ASSERT_NEAR(br_ray.origin.z, -near_plane, 1e-4f);
        ASSERT_NEAR(glm::length(br_ray.direction), 1.0f, 1e-5f);
    }

    // =========================================================================
    // Test 2: Mathematical Unprojection with Perspective Projection & Normalization
    // =========================================================================
    {
        const int width = 1200;
        const int height = 800;
        const glm::vec3 cam_pos(0.0f, 5.0f, 15.0f);
        const glm::vec3 cam_target(0.0f, 0.0f, 0.0f);
        const glm::vec3 cam_up(0.0f, 1.0f, 0.0f);

        glm::mat4 view = glm::lookAt(cam_pos, cam_target, cam_up);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 1000.0f);
        glm::mat4 inv_vp = glm::inverse(proj * view);

        // Center ray should point directly toward target
        Ray center_ray = RaycastPicker::unproject(600.0f, 400.0f, width, height, inv_vp);
        glm::vec3 expected_dir = glm::normalize(cam_target - cam_pos);
        ASSERT_VEC3_NEAR(center_ray.direction, expected_dir, 1e-3f);
        ASSERT_NEAR(glm::length(center_ray.direction), 1.0f, 1e-5f);

        // All rays across screen must have unit length ||d|| == 1.0
        const float sample_xs[] = {0.0f, 300.0f, 600.0f, 900.0f, 1200.0f};
        const float sample_ys[] = {0.0f, 200.0f, 400.0f, 600.0f, 800.0f};
        for (float sx : sample_xs) {
            for (float sy : sample_ys) {
                Ray r = RaycastPicker::unproject(sx, sy, width, height, inv_vp);
                float len = glm::length(r.direction);
                ASSERT_NEAR(len, 1.0f, 1e-5f);
            }
        }

        // Corner rays should diverge symmetrically from center
        Ray tl = RaycastPicker::unproject(0.0f, 0.0f, width, height, inv_vp);
        Ray tr = RaycastPicker::unproject(1200.0f, 0.0f, width, height, inv_vp);
        Ray bl = RaycastPicker::unproject(0.0f, 800.0f, width, height, inv_vp);
        Ray br = RaycastPicker::unproject(1200.0f, 800.0f, width, height, inv_vp);

        ASSERT_TRUE(tl.direction.x < 0.0f);
        ASSERT_TRUE(tl.direction.y > 0.0f);
        ASSERT_TRUE(tr.direction.x > 0.0f);
        ASSERT_TRUE(tr.direction.y > 0.0f);
        ASSERT_TRUE(bl.direction.x < 0.0f);
        ASSERT_TRUE(bl.direction.y < 0.0f);
        ASSERT_TRUE(br.direction.x > 0.0f);
        ASSERT_TRUE(br.direction.y < 0.0f);

        // Edge case: invalid window dimensions return safe fallback
        Ray zero_dim = RaycastPicker::unproject(100.0f, 100.0f, 0, 0, inv_vp);
        ASSERT_NEAR(glm::length(zero_dim.direction), 1.0f, 1e-5f);
    }

    // =========================================================================
    // Test 3: Camera Instance Unprojection Overload
    // =========================================================================
    {
        Camera camera;
        camera.position = glm::vec3(5.0f, 10.0f, 20.0f);
        camera.aim(0.0, 0.0);
        camera.update();

        Ray r = RaycastPicker::unproject(400.0f, 300.0f, 800, 600, &camera);
        ASSERT_NEAR(glm::length(r.direction), 1.0f, 1e-5f);

        // Null camera safety
        Ray null_cam_ray = RaycastPicker::unproject(400.0f, 300.0f, 800, 600, static_cast<const Camera*>(nullptr));
        ASSERT_NEAR(glm::length(null_cam_ray.direction), 1.0f, 1e-5f);
    }

    // =========================================================================
    // Test 4: Bullet Dynamic Raycast Picking with Simulation Instance
    // =========================================================================
    {
        RaycastTestSimulation sim;
        ASSERT_TRUE(sim.dynamics_world != nullptr);

        // 1. Add static ground plane (mass = 0)
        scenegraph::TransformNode ground_node;
        ground_node.name = "StaticGround";
        ground_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        btBoxShape* ground_shape = new btBoxShape(btVector3(50.0f, 0.5f, 50.0f));
        sim.addPhysicsNode(&ground_node, ground_shape, 0.0f, 0.0f, -0.5f, 0.0f);

        // 2. Add dynamic sphere (mass = 3.0, radius = 1.0) centered at (0, 5, 0)
        scenegraph::TransformNode sphere_node;
        sphere_node.name = "TargetSphere";
        sphere_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f));
        btSphereShape* sphere_shape = new btSphereShape(1.0f);
        sim.addPhysicsNode(&sphere_node, sphere_shape, 3.0f, 0.0f, 0.0f, 0.0f);

        ASSERT_TRUE(sim.rigid_bodies.find("TargetSphere") != sim.rigid_bodies.end());
        btRigidBody* target_body = sim.rigid_bodies["TargetSphere"];
        ASSERT_TRUE(target_body != nullptr);

        RaycastPicker picker(sim.dynamics_world);

        // A. Ray directed straight at the sphere: origin (0, 10, 0), direction (0, -1, 0)
        Ray direct_hit_ray{glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
        bool hit = picker.pick(direct_hit_ray, 100.0f);

        ASSERT_TRUE(hit);
        ASSERT_TRUE(picker.isDragging());
        ASSERT_TRUE(picker.getPickedBody() == target_body);
        ASSERT_TRUE(picker.getConstraint() != nullptr);

        // Top of sphere is at y = 6.0, origin at y = 10.0 => distance should be ~4.0
        ASSERT_NEAR(picker.getHitDistance(), 4.0f, 0.05f);

        // Local pivot in sphere local coordinates: top surface is +Y = 1.0
        btVector3 local_pivot = picker.getLocalPivot();
        ASSERT_NEAR(local_pivot.x(), 0.0f, 0.05f);
        ASSERT_NEAR(local_pivot.y(), 1.0f, 0.05f);
        ASSERT_NEAR(local_pivot.z(), 0.0f, 0.05f);

        // Verify constraint was registered in dynamics world
        ASSERT_TRUE(sim.dynamics_world->getNumConstraints() == 1);

        // B. Dragging pivot update
        Ray drag_ray{glm::vec3(0.0f, 10.0f, 0.0f), glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f))};
        picker.drag(drag_ray);

        glm::vec3 expected_drag_target = drag_ray.origin + drag_ray.direction * picker.getHitDistance();
        btPoint2PointConstraint* p2p = picker.getConstraint();
        ASSERT_TRUE(p2p != nullptr);
        ASSERT_NEAR(p2p->getPivotInB().x(), expected_drag_target.x, 1e-4f);
        ASSERT_NEAR(p2p->getPivotInB().y(), expected_drag_target.y, 1e-4f);
        ASSERT_NEAR(p2p->getPivotInB().z(), expected_drag_target.z, 1e-4f);

        // C. Clean release
        picker.release();
        ASSERT_FALSE(picker.isDragging());
        ASSERT_TRUE(picker.getPickedBody() == nullptr);
        ASSERT_TRUE(picker.getConstraint() == nullptr);
        ASSERT_TRUE(sim.dynamics_world->getNumConstraints() == 0);
    }

    // =========================================================================
    // Test 5: Raycast Filtering & Non-Dynamic Objects
    // =========================================================================
    {
        RaycastTestSimulation sim;

        // Static object (mass = 0)
        scenegraph::TransformNode static_node;
        static_node.name = "StaticWall";
        static_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
        btBoxShape* box_shape = new btBoxShape(btVector3(2.0f, 2.0f, 0.5f));
        sim.addPhysicsNode(&static_node, box_shape, 0.0f, 0.0f, 0.0f, 0.0f);

        RaycastPicker picker(sim.dynamics_world);

        // Ray hitting the static wall
        Ray ray_to_static{glm::vec3(0.0f, 2.0f, 10.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
        bool hit_static = picker.pick(ray_to_static, 50.0f);

        // Picker must reject static objects (cannot drag world geometry)
        ASSERT_FALSE(hit_static);
        ASSERT_FALSE(picker.isDragging());
        ASSERT_TRUE(picker.getPickedBody() == nullptr);
        ASSERT_TRUE(picker.getConstraint() == nullptr);

        // Ray missing all objects completely
        Ray miss_ray{glm::vec3(500.0f, 500.0f, 500.0f), glm::vec3(0.0f, 1.0f, 0.0f)};
        bool hit_miss = picker.pick(miss_ray, 100.0f);
        ASSERT_FALSE(hit_miss);
        ASSERT_FALSE(picker.isDragging());
    }

    // =========================================================================
    // Test 6: RAII Destructor & Move Semantics Memory Safety
    // =========================================================================
    {
        RaycastTestSimulation sim;
        scenegraph::TransformNode sphere_node;
        sphere_node.name = "MovableSphere";
        sphere_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, 0.0f));
        btSphereShape* sphere_shape = new btSphereShape(1.0f);
        sim.addPhysicsNode(&sphere_node, sphere_shape, 1.0f, 0.0f, 0.0f, 0.0f);

        Ray ray{glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)};

        // RAII scope exit: picker destroyed while holding a constraint
        {
            RaycastPicker temp_picker(sim.dynamics_world);
            bool picked = temp_picker.pick(ray, 50.0f);
            ASSERT_TRUE(picked);
            ASSERT_TRUE(sim.dynamics_world->getNumConstraints() == 1);
        }
        // Constraint must be automatically removed without leaks
        ASSERT_TRUE(sim.dynamics_world->getNumConstraints() == 0);

        // Move semantics test
        {
            RaycastPicker picker_a(sim.dynamics_world);
            ASSERT_TRUE(picker_a.pick(ray, 50.0f));
            ASSERT_TRUE(picker_a.isDragging());

            RaycastPicker picker_b(std::move(picker_a));
            ASSERT_FALSE(picker_a.isDragging());
            ASSERT_TRUE(picker_a.getPickedBody() == nullptr);
            ASSERT_TRUE(picker_b.isDragging());
            ASSERT_TRUE(picker_b.getPickedBody() != nullptr);
            ASSERT_TRUE(sim.dynamics_world->getNumConstraints() == 1);

            picker_b.release();
            ASSERT_FALSE(picker_b.isDragging());
            ASSERT_TRUE(sim.dynamics_world->getNumConstraints() == 0);
        }
    }

    return failures;
}
