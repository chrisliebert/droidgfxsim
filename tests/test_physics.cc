#include "test_helpers.h"
#include "physics/simulation.h"
#include "physics/terrain.h"
#include "common/asset_manager.hpp"
#include "application/application.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>

// Derived test harness to expose protected Simulation methods and provide
// shape helpers (addPhysicsSphereNode, addPhysicsCylinderNode)
class PhysicsTestSimulation : public Simulation {
public:
    using Simulation::addPhysicsNode;
    using Simulation::addPhysicsBoxNode;
    using Simulation::addPhysicsConvexHullNode;
};

int runPhysicsTests() {
    int failures = 0;

    // =========================================================================
    // Test 1: Simulation Initialization & World Configuration
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        ASSERT_TRUE(sim.dynamics_world != nullptr);

        sim.dynamics_world->setGravity(btVector3(0.0f, -9.81f, 0.0f));
        btVector3 grav = sim.dynamics_world->getGravity();
        ASSERT_NEAR(grav.x(), 0.0f, 1e-4f);
        ASSERT_NEAR(grav.y(), -9.81f, 1e-4f);
        ASSERT_NEAR(grav.z(), 0.0f, 1e-4f);
    }

    // =========================================================================
    // Test 2: Sphere Collision Shape Creation (addPhysicsSphereNode) & Inertia
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        scenegraph::TransformNode sphere_node;
        sphere_node.name = "DynamicSphere";
        sphere_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 10.0f, 2.0f));

        const float mass = 2.5f;
        const float radius = 1.5f;
        sim.addPhysicsSphereNode(&sphere_node, mass, radius);

        // Verify registration in rigid_bodies and collision_shapes
        ASSERT_TRUE(sim.rigid_bodies.find("DynamicSphere") != sim.rigid_bodies.end());
        btRigidBody* sphere_body = sim.rigid_bodies["DynamicSphere"];
        ASSERT_TRUE(sphere_body != nullptr);

        // Verify shape properties
        btCollisionShape* shape = sphere_body->getCollisionShape();
        ASSERT_TRUE(shape != nullptr);
        ASSERT_TRUE(shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE);
        btSphereShape* sphere_shape = static_cast<btSphereShape*>(shape);
        ASSERT_NEAR(sphere_shape->getRadius(), radius, 1e-4f);

        // Verify mass and inverse mass
        ASSERT_NEAR(sphere_body->getInvMass(), 1.0f / mass, 1e-4f);

        // Verify analytical solid sphere inertia: I = 2/5 * M * R^2 along all 3 axes
        const float expected_inertia_val = 0.4f * mass * radius * radius;
        btVector3 computed_inertia(0.0f, 0.0f, 0.0f);
        sphere_shape->calculateLocalInertia(mass, computed_inertia);
        ASSERT_NEAR(computed_inertia.x(), expected_inertia_val, 1e-4f);
        ASSERT_NEAR(computed_inertia.y(), expected_inertia_val, 1e-4f);
        ASSERT_NEAR(computed_inertia.z(), expected_inertia_val, 1e-4f);

        // Verify rigid body inverse inertia matches analytical inertia
        btVector3 inv_inertia = sphere_body->getInvInertiaDiagLocal();
        ASSERT_NEAR(inv_inertia.x(), 1.0f / expected_inertia_val, 1e-4f);
        ASSERT_NEAR(inv_inertia.y(), 1.0f / expected_inertia_val, 1e-4f);
        ASSERT_NEAR(inv_inertia.z(), 1.0f / expected_inertia_val, 1e-4f);

        // Verify initial world transform origin
        btTransform trans;
        sphere_body->getMotionState()->getWorldTransform(trans);
        ASSERT_NEAR(trans.getOrigin().x(), 1.0f, 1e-4f);
        ASSERT_NEAR(trans.getOrigin().y(), 10.0f, 1e-4f);
        ASSERT_NEAR(trans.getOrigin().z(), 2.0f, 1e-4f);
    }

    // =========================================================================
    // Test 3: Cylinder Collision Shape Creation (addPhysicsCylinderNode) & Inertia
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        scenegraph::TransformNode cylinder_node;
        cylinder_node.name = "DynamicCylinder";
        cylinder_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 5.0f, -1.0f));

        const float mass = 3.0f;
        const float radius = 0.8f;
        const float height = 2.0f;
        sim.addPhysicsCylinderNode(&cylinder_node, mass, radius, height);

        // Verify registration in rigid_bodies
        ASSERT_TRUE(sim.rigid_bodies.find("DynamicCylinder") != sim.rigid_bodies.end());
        btRigidBody* cylinder_body = sim.rigid_bodies["DynamicCylinder"];
        ASSERT_TRUE(cylinder_body != nullptr);

        // Verify shape properties
        btCollisionShape* shape = cylinder_body->getCollisionShape();
        ASSERT_TRUE(shape != nullptr);
        ASSERT_TRUE(shape->getShapeType() == CYLINDER_SHAPE_PROXYTYPE);
        btCylinderShape* cylinder_shape = static_cast<btCylinderShape*>(shape);
        ASSERT_NEAR(cylinder_shape->getRadius(), radius, 1e-4f);

        // Verify mass
        ASSERT_NEAR(cylinder_body->getInvMass(), 1.0f / mass, 1e-4f);

        // Verify analytical cylinder inertia (Bullet default Y-axis orientation):
        // Iy = 1/2 * M * R^2
        // Ix = Iz = 1/12 * M * (3 * R^2 + H^2)
        const float expected_iy = 0.5f * mass * (radius * radius);
        const float expected_ixz = (mass / 12.0f) * (3.0f * (radius * radius) + (height * height));

        btVector3 computed_inertia(0.0f, 0.0f, 0.0f);
        cylinder_shape->calculateLocalInertia(mass, computed_inertia);
        ASSERT_NEAR(computed_inertia.y(), expected_iy, 1e-3f);
        ASSERT_NEAR(computed_inertia.x(), expected_ixz, 1e-3f);
        ASSERT_NEAR(computed_inertia.z(), expected_ixz, 1e-3f);
        ASSERT_NEAR(computed_inertia.x(), computed_inertia.z(), 1e-4f);

        // Verify rigid body inverse inertia
        btVector3 inv_inertia = cylinder_body->getInvInertiaDiagLocal();
        ASSERT_NEAR(inv_inertia.y(), 1.0f / expected_iy, 1e-3f);
        ASSERT_NEAR(inv_inertia.x(), 1.0f / expected_ixz, 1e-3f);
        ASSERT_NEAR(inv_inertia.z(), 1.0f / expected_ixz, 1e-3f);
    }

    // =========================================================================
    // Test 4: Convex Hull Shape Creation with Multiple Vertices
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        scenegraph::TransformNode hull_node;
        hull_node.name = "ConvexHullMesh";
        hull_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));

        scenegraph::GeometryNode geom_child;
        geom_child.name = "HullGeometry";

        // Create an octahedron with 6 vertices
        scenegraph::Vertex v0 = {{ 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v1 = {{ 0.0f, -1.0f,  0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v2 = {{ 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v3 = {{-1.0f,  0.0f,  0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v4 = {{ 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v5 = {{ 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}};

        geom_child.vertex_data = {v0, v1, v2, v3, v4, v5};
        // 8 triangular faces of an octahedron (24 indices total)
        geom_child.index_data = {
            0, 2, 4,   0, 4, 3,   0, 3, 5,   0, 5, 2,
            1, 4, 2,   1, 3, 4,   1, 5, 3,   1, 2, 5
        };

        hull_node.children.push_back(&geom_child);

        const float mass = 4.0f;
        sim.addPhysicsConvexHullNode(&hull_node, mass);

        // Verify shape creation and vertex population
        ASSERT_TRUE(sim.convex_hull_shapes.find("ConvexHullMesh") != sim.convex_hull_shapes.end());
        btConvexHullShape* hull_shape = sim.convex_hull_shapes["ConvexHullMesh"];
        ASSERT_TRUE(hull_shape != nullptr);
        ASSERT_TRUE(hull_shape->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE);

        // In populateConvexHullShapeFromNode, points are added for each index
        ASSERT_TRUE(hull_shape->getNumPoints() > 0);
        ASSERT_TRUE(hull_shape->getNumPoints() == 24);

        // Verify rigid body registration
        ASSERT_TRUE(sim.rigid_bodies.find("ConvexHullMesh") != sim.rigid_bodies.end());
        btRigidBody* hull_body = sim.rigid_bodies["ConvexHullMesh"];
        ASSERT_TRUE(hull_body != nullptr);
        ASSERT_NEAR(hull_body->getInvMass(), 1.0f / mass, 1e-4f);
    }

    // =========================================================================
    // Test 5: Multi-Body Stepping: Gravity Fall and Ground Collision
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        sim.dynamics_world->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        // 1. Static Ground Plane (mass = 0)
        // Center at y = -0.5, half-height = 0.5, so top surface is at y = 0.0
        scenegraph::TransformNode ground_node;
        ground_node.name = "Ground";
        ground_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
        sim.addPhysicsBoxNode(&ground_node, 0.0f, 20.0f, 0.5f, 20.0f);

        // 2. Dynamic Falling Box (mass = 1.0, half-extents 0.5)
        scenegraph::TransformNode box_node;
        box_node.name = "FallingBox";
        box_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 5.0f, 0.0f));
        sim.addPhysicsBoxNode(&box_node, 1.0f, 0.5f, 0.5f, 0.5f);

        // 3. Dynamic Falling Sphere (mass = 1.5, radius = 0.5)
        scenegraph::TransformNode sphere_node;
        sphere_node.name = "FallingSphere";
        sphere_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 6.0f, 0.0f));
        sim.addPhysicsSphereNode(&sphere_node, 1.5f, 0.5f);

        // 4. Dynamic Falling Cylinder (mass = 2.0, radius = 0.5, height = 1.0)
        scenegraph::TransformNode cylinder_node;
        cylinder_node.name = "FallingCylinder";
        cylinder_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 7.0f, 0.0f));
        sim.addPhysicsCylinderNode(&cylinder_node, 2.0f, 0.5f, 1.0f);

        // Ensure 4 rigid bodies in dynamics world
        ASSERT_TRUE(sim.dynamics_world->getNumCollisionObjects() == 4);

        btTransform trans;
        sim.rigid_bodies["FallingBox"]->getMotionState()->getWorldTransform(trans);
        const float init_box_y = trans.getOrigin().y();

        sim.rigid_bodies["FallingSphere"]->getMotionState()->getWorldTransform(trans);
        const float init_sphere_y = trans.getOrigin().y();

        sim.rigid_bodies["FallingCylinder"]->getMotionState()->getWorldTransform(trans);
        const float init_cylinder_y = trans.getOrigin().y();

        // Step simulation for a few ticks to verify free fall under gravity
        for (int i = 0; i < 10; ++i) {
            sim.dynamics_world->stepSimulation(1.0f / 60.0f, 10);
        }

        // Verify downward displacement and negative vertical velocity
        sim.rigid_bodies["FallingBox"]->getMotionState()->getWorldTransform(trans);
        ASSERT_TRUE(trans.getOrigin().y() < init_box_y);
        ASSERT_TRUE(sim.rigid_bodies["FallingBox"]->getLinearVelocity().y() < 0.0f);

        sim.rigid_bodies["FallingSphere"]->getMotionState()->getWorldTransform(trans);
        ASSERT_TRUE(trans.getOrigin().y() < init_sphere_y);
        ASSERT_TRUE(sim.rigid_bodies["FallingSphere"]->getLinearVelocity().y() < 0.0f);

        sim.rigid_bodies["FallingCylinder"]->getMotionState()->getWorldTransform(trans);
        ASSERT_TRUE(trans.getOrigin().y() < init_cylinder_y);
        ASSERT_TRUE(sim.rigid_bodies["FallingCylinder"]->getLinearVelocity().y() < 0.0f);

        // Step simulation for 180 ticks (3.0 seconds) to allow bodies to collide with ground
        for (int i = 0; i < 180; ++i) {
            sim.dynamics_world->stepSimulation(1.0f / 60.0f, 10);
        }

        // Verify bodies collided and rest above or on the static ground plane (y = 0.0)
        // With half-extents/radii of 0.5, resting positions should be near y = 0.5
        sim.rigid_bodies["FallingBox"]->getMotionState()->getWorldTransform(trans);
        const float final_box_y = trans.getOrigin().y();
        ASSERT_TRUE(final_box_y >= 0.40f && final_box_y <= 1.0f);

        sim.rigid_bodies["FallingSphere"]->getMotionState()->getWorldTransform(trans);
        const float final_sphere_y = trans.getOrigin().y();
        ASSERT_TRUE(final_sphere_y >= 0.40f && final_sphere_y <= 1.0f);

        sim.rigid_bodies["FallingCylinder"]->getMotionState()->getWorldTransform(trans);
        const float final_cylinder_y = trans.getOrigin().y();
        ASSERT_TRUE(final_cylinder_y >= 0.40f && final_cylinder_y <= 1.0f);

        // Verify contact manifolds exist between the dynamic bodies and the ground
        int num_manifolds = sim.dynamics_world->getDispatcher()->getNumManifolds();
        ASSERT_TRUE(num_manifolds > 0);

        int total_contacts = 0;
        for (int m = 0; m < num_manifolds; ++m) {
            btPersistentManifold* contact_manifold = sim.dynamics_world->getDispatcher()->getManifoldByIndexInternal(m);
            if (contact_manifold) {
                total_contacts += contact_manifold->getNumContacts();
            }
        }
        ASSERT_TRUE(total_contacts > 0);
    }

    // =========================================================================
    // Test 6: Convex Hull Local Space Alignment (Regression Test)
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        scenegraph::TransformNode hull_node;
        hull_node.name = "AlignedConvexHull";
        hull_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 10.0f, -3.0f));

        scenegraph::GeometryNode geom_child;
        geom_child.name = "OctahedronGeom";

        // Octahedron vertices bounded in [-1.0, 1.0]
        scenegraph::Vertex v0 = {{ 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v1 = {{ 0.0f, -1.0f,  0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v2 = {{ 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v3 = {{-1.0f,  0.0f,  0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v4 = {{ 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}};
        scenegraph::Vertex v5 = {{ 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}};

        geom_child.vertex_data = {v0, v1, v2, v3, v4, v5};
        // 8 triangular faces (24 indices total)
        geom_child.index_data = {
            0, 2, 4,   0, 4, 3,   0, 3, 5,   0, 5, 2,
            1, 4, 2,   1, 3, 4,   1, 5, 3,   1, 2, 5
        };

        hull_node.children.push_back(&geom_child);

        const float mass = 2.0f;
        sim.addPhysicsConvexHullNode(&hull_node, mass);

        // Verify shape creation
        ASSERT_TRUE(sim.convex_hull_shapes.find("AlignedConvexHull") != sim.convex_hull_shapes.end());
        btConvexHullShape* hull_shape = sim.convex_hull_shapes["AlignedConvexHull"];
        ASSERT_TRUE(hull_shape != nullptr);
        ASSERT_TRUE(hull_shape->getNumPoints() == 24);

        // Verify that all points in the convex hull shape remain in local space [-1, 1],
        // and are NOT shifted by the parent TransformNode's translation (5.0, 10.0, -3.0).
        for (int i = 0; i < hull_shape->getNumPoints(); ++i) {
            btVector3 pt = hull_shape->getScaledPoint(i);
            ASSERT_TRUE(pt.x() >= -1.01f && pt.x() <= 1.01f);
            ASSERT_TRUE(pt.y() >= -1.01f && pt.y() <= 1.01f);
            ASSERT_TRUE(pt.z() >= -1.01f && pt.z() <= 1.01f);
        }

        // Verify local AABB is centered around (0, 0, 0) in local frame
        btTransform identity_trans;
        identity_trans.setIdentity();
        btVector3 aabb_min, aabb_max;
        hull_shape->getAabb(identity_trans, aabb_min, aabb_max);

        // Bounds should be roughly within [-1.1, 1.1] (accounting for collision margin)
        ASSERT_TRUE(aabb_min.x() >= -1.1f && aabb_min.x() <= -0.9f);
        ASSERT_TRUE(aabb_min.y() >= -1.1f && aabb_min.y() <= -0.9f);
        ASSERT_TRUE(aabb_min.z() >= -1.1f && aabb_min.z() <= -0.9f);
        ASSERT_TRUE(aabb_max.x() >=  0.9f && aabb_max.x() <=  1.1f);
        ASSERT_TRUE(aabb_max.y() >=  0.9f && aabb_max.y() <=  1.1f);
        ASSERT_TRUE(aabb_max.z() >=  0.9f && aabb_max.z() <=  1.1f);

        // Verify local AABB is NOT shifted by (5.0, 10.0, -3.0)
        ASSERT_TRUE(aabb_max.x() < 3.0f);
        ASSERT_TRUE(aabb_max.y() < 5.0f);
        ASSERT_TRUE(aabb_min.z() > -2.0f);

        // Verify rigid body world transform origin is at (5.0, 10.0, -3.0)
        ASSERT_TRUE(sim.rigid_bodies.find("AlignedConvexHull") != sim.rigid_bodies.end());
        btRigidBody* hull_body = sim.rigid_bodies["AlignedConvexHull"];
        ASSERT_TRUE(hull_body != nullptr);

        btTransform world_trans;
        hull_body->getMotionState()->getWorldTransform(world_trans);
        ASSERT_NEAR(world_trans.getOrigin().x(), 5.0f, 1e-4f);
        ASSERT_NEAR(world_trans.getOrigin().y(), 10.0f, 1e-4f);
        ASSERT_NEAR(world_trans.getOrigin().z(), -3.0f, 1e-4f);

        ASSERT_NEAR(hull_body->getWorldTransform().getOrigin().x(), 5.0f, 1e-4f);
        ASSERT_NEAR(hull_body->getWorldTransform().getOrigin().y(), 10.0f, 1e-4f);
        ASSERT_NEAR(hull_body->getWorldTransform().getOrigin().z(), -3.0f, 1e-4f);
    }

    // =========================================================================
    // Test 7: Static Convex Hull Collision
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        sim.dynamics_world->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        // Create a static convex hull platform (mass = 0.0) centered at (0.0, 1.0, 0.0)
        scenegraph::TransformNode platform_node;
        platform_node.name = "StaticConvexHullPlatform";
        platform_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        scenegraph::GeometryNode platform_geom;
        platform_geom.name = "PlatformGeom";

        // Box slab with half-extents: x=2.0, y=0.25, z=2.0 (local y top at +0.25 -> world y top at 1.25)
        const float hx = 2.0f;
        const float hy = 0.25f;
        const float hz = 2.0f;
        platform_geom.vertex_data = {
            {{-hx, -hy, -hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ hx, -hy, -hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ hx,  hy, -hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-hx,  hy, -hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-hx, -hy,  hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ hx, -hy,  hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{ hx,  hy,  hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-hx,  hy,  hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}
        };
        platform_geom.index_data = {
            4, 5, 6,  4, 6, 7, // Front (+Z)
            1, 0, 3,  1, 3, 2, // Back (-Z)
            3, 2, 6,  3, 6, 7, // Top (+Y)
            0, 1, 5,  0, 5, 4, // Bottom (-Y)
            1, 2, 6,  1, 6, 5, // Right (+X)
            0, 4, 7,  0, 7, 3  // Left (-X)
        };
        platform_node.children.push_back(&platform_geom);

        sim.addPhysicsConvexHullNode(&platform_node, 0.0f); // Static platform (mass = 0)

        // Create a dynamic falling sphere dropping directly above the platform
        scenegraph::TransformNode sphere_node;
        sphere_node.name = "DroppingSphere";
        sphere_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f));

        const float sphere_mass = 1.0f;
        const float sphere_radius = 0.5f;
        sim.addPhysicsSphereNode(&sphere_node, sphere_mass, sphere_radius);

        ASSERT_TRUE(sim.dynamics_world->getNumCollisionObjects() == 2);

        btTransform trans;
        sim.rigid_bodies["DroppingSphere"]->getMotionState()->getWorldTransform(trans);
        const float init_sphere_y = trans.getOrigin().y();
        ASSERT_NEAR(init_sphere_y, 5.0f, 1e-4f);

        // Step simulation for 10 ticks to verify free fall under gravity
        for (int i = 0; i < 10; ++i) {
            sim.dynamics_world->stepSimulation(1.0f / 60.0f, 10);
        }

        sim.rigid_bodies["DroppingSphere"]->getMotionState()->getWorldTransform(trans);
        ASSERT_TRUE(trans.getOrigin().y() < init_sphere_y);
        ASSERT_TRUE(sim.rigid_bodies["DroppingSphere"]->getLinearVelocity().y() < 0.0f);

        // Step simulation for 180 ticks (3.0 seconds) to allow the sphere to impact and settle
        for (int i = 0; i < 180; ++i) {
            sim.dynamics_world->stepSimulation(1.0f / 60.0f, 10);
        }

        // Verify the sphere came to rest on top of the static convex hull platform
        // Platform top is at y = 1.0 (origin) + 0.25 (half-height) = 1.25.
        // Sphere resting center should be near y = 1.25 + 0.5 = 1.75.
        sim.rigid_bodies["DroppingSphere"]->getMotionState()->getWorldTransform(trans);
        const float final_sphere_y = trans.getOrigin().y();
        ASSERT_TRUE(final_sphere_y >= 1.65f && final_sphere_y <= 1.95f);
        ASSERT_TRUE(std::abs(sim.rigid_bodies["DroppingSphere"]->getLinearVelocity().y()) < 0.1f);

        // Verify contact manifold exists between the sphere and the static convex hull
        int num_manifolds = sim.dynamics_world->getDispatcher()->getNumManifolds();
        ASSERT_TRUE(num_manifolds > 0);

        int total_contacts = 0;
        for (int m = 0; m < num_manifolds; ++m) {
            btPersistentManifold* contact_manifold = sim.dynamics_world->getDispatcher()->getManifoldByIndexInternal(m);
            if (contact_manifold) {
                total_contacts += contact_manifold->getNumContacts();
            }
        }
        ASSERT_TRUE(total_contacts > 0);
    }

    // =========================================================================
    // Test 8: Multi-Tiered Cascade and Restitution Scattering
    // =========================================================================
    {
        PhysicsTestSimulation sim;
        sim.dynamics_world->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        // Create an inclined static ramp (restitution 0.75)
        scenegraph::TransformNode ramp_node;
        ramp_node.name = "CascadeInclinedRamp";
        ramp_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 8.5f, 0.0f));
        sim.addPhysicsBoxNode(&ramp_node, 0.0f, 30.0f, 1.5f, 30.0f);

        btRigidBody* ramp_body = sim.rigid_bodies["CascadeInclinedRamp"];
        ASSERT_TRUE(ramp_body != nullptr);

        // Incline the ramp by rotating around Z axis (-0.4 radians ~ -22.9 degrees)
        btTransform ramp_trans = ramp_body->getWorldTransform();
        ramp_trans.setRotation(btQuaternion(btVector3(0.0f, 0.0f, 1.0f), -0.4f));
        ramp_body->setWorldTransform(ramp_trans);
        if (ramp_body->getMotionState()) {
            ramp_body->getMotionState()->setWorldTransform(ramp_trans);
        }
        ramp_body->setRestitution(0.75f);
        ramp_body->setFriction(0.2f);

        // Spawn 8 dynamic spheres stacked vertically at staggered elevations Y = 12, 18, 25, 32, 40, 48, 56, 65 (restitution 0.80)
        const std::vector<float> elevations = {12.0f, 18.0f, 25.0f, 32.0f, 40.0f, 48.0f, 56.0f, 65.0f};
        const int num_spheres = static_cast<int>(elevations.size());
        std::vector<scenegraph::TransformNode> sphere_nodes(num_spheres);
        std::vector<std::string> sphere_names(num_spheres);

        for (int i = 0; i < num_spheres; ++i) {
            sphere_names[i] = "CascadeSphere_" + std::to_string(i);
            sphere_nodes[i].name = sphere_names[i];
            // Stacked vertically above the inclined ramp section
            sphere_nodes[i].matrix = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, elevations[i], 0.0f));

            const float sphere_mass = 2.0f;
            const float sphere_radius = 0.6f;
            sim.addPhysicsSphereNode(&sphere_nodes[i], sphere_mass, sphere_radius);

            btRigidBody* body = sim.rigid_bodies[sphere_names[i]];
            ASSERT_TRUE(body != nullptr);
            body->setRestitution(0.80f);
            body->setFriction(0.2f);
            body->setDamping(0.0f, 0.0f);
            body->setRollingFriction(0.0f);
            // Enable continuous collision detection to eliminate tunneling under high cascade velocities
            body->setCcdMotionThreshold(0.1f);
            body->setCcdSweptSphereRadius(0.3f);
        }

        ASSERT_TRUE(sim.dynamics_world->getNumCollisionObjects() == num_spheres + 1);

        std::vector<float> max_horiz_vel(num_spheres, 0.0f);
        std::vector<int> bounce_count(num_spheres, 0);
        std::vector<float> prev_vy(num_spheres, 0.0f);
        std::vector<bool> tunneled(num_spheres, false);

        btTransform ramp_trans_inv = ramp_body->getWorldTransform().inverse();

        // Step the simulation for 200 physics steps (over 3.3 seconds at 60Hz)
        for (int step = 0; step < 200; ++step) {
            sim.dynamics_world->stepSimulation(1.0f / 60.0f, 10);

            for (int i = 0; i < num_spheres; ++i) {
                btRigidBody* body = sim.rigid_bodies[sphere_names[i]];
                btTransform trans;
                body->getMotionState()->getWorldTransform(trans);
                btVector3 vel = body->getLinearVelocity();

                // Ball must not tunnel through the ramp: in ramp local coordinates, ball must remain above ramp bottom
                btVector3 local_pos = ramp_trans_inv(trans.getOrigin());
                if (std::abs(local_pos.x()) <= 30.0f && std::abs(local_pos.z()) <= 30.0f) {
                    if (local_pos.y() < -1.5f - 0.6f) {
                        tunneled[i] = true;
                    }
                }

                float horiz_vel = std::max(std::abs(vel.x()), std::abs(vel.z()));
                if (horiz_vel > max_horiz_vel[i]) {
                    max_horiz_vel[i] = horiz_vel;
                }

                // Detect bounce: transition from downward fall to upward rebound
                if (prev_vy[i] < -0.5f && vel.y() > 0.5f) {
                    bounce_count[i]++;
                }
                prev_vy[i] = vel.y();
            }
        }

        // Calculate final kinetic energy of the system
        float total_final_ke = 0.0f;
        int spheres_with_high_horizontal_vel = 0;
        int total_bounces = 0;

        for (int i = 0; i < num_spheres; ++i) {
            btRigidBody* body = sim.rigid_bodies[sphere_names[i]];
            btVector3 vel = body->getLinearVelocity();
            float ke = 0.5f * 2.0f * vel.length2();
            total_final_ke += ke;

            total_bounces += bounce_count[i];
            if (max_horiz_vel[i] > 2.0f) {
                spheres_with_high_horizontal_vel++;
            }
            ASSERT_TRUE(!tunneled[i]);
        }

        // Assert that balls bounce vigorously
        ASSERT_TRUE(total_bounces >= 8);

        // Assert that balls gain substantial horizontal velocity (vx > 2.0 m/s or vz > 2.0 m/s)
        ASSERT_TRUE(spheres_with_high_horizontal_vel == num_spheres);

        // Assert kinetic energy is maintained through multiple bounces
        ASSERT_TRUE(total_final_ke > 5.0f);
    }

    // =========================================================================
    // Test 9: Attenuated Restitution & Laminar Chute Non-Explosive Decay
    // =========================================================================
    {
        PhysicsTestSimulation sim;

        // Static ground arena with default attenuated restitution
        scenegraph::TransformNode ground_node;
        ground_node.name = "Arena_Floor";
        ground_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        sim.addPhysicsBoxNode(&ground_node, 0.0f, 20.0f, 0.2f, 20.0f);

        btRigidBody* floor_body = sim.rigid_bodies["Arena_Floor"];
        ASSERT_TRUE(floor_body != nullptr);
        ASSERT_TRUE(floor_body->getRestitution() <= 0.25f);
        ASSERT_TRUE(floor_body->getFriction() >= 0.45f);

        // Dynamic falling sphere with engine defaults (attenuated restitution, rolling friction, damping)
        scenegraph::TransformNode sphere_node;
        sphere_node.name = "ChuteSphere";
        sphere_node.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 8.0f, 0.0f));
        sim.addPhysicsSphereNode(&sphere_node, 2.0f, 0.5f);

        btRigidBody* sphere_body = sim.rigid_bodies["ChuteSphere"];
        ASSERT_TRUE(sphere_body != nullptr);
        ASSERT_TRUE(sphere_body->getRestitution() <= 0.30f);
        ASSERT_TRUE(sphere_body->getRollingFriction() >= 0.05f);

        // Step simulation for 180 steps (3 seconds at 60Hz)
        float max_rebound_vy = 0.0f;
        for (int step = 0; step < 180; ++step) {
            sim.dynamics_world->stepSimulation(1.0f / 60.0f, 10);
            btVector3 vel = sphere_body->getLinearVelocity();
            if (vel.y() > max_rebound_vy) {
                max_rebound_vy = vel.y();
            }
        }

        // With attenuated restitution (e <= 0.28, floor e = 0.20), impact kinetic energy is absorbed
        // Rebound upward velocity is subdued (vy < 1.5 m/s, unlike explosive bounce > 8 m/s)
        ASSERT_TRUE(max_rebound_vy < 1.5f);

        // Verify resting stability near y = 0.7m (0.2m floor half-height + 0.5m radius)
        btTransform final_trans;
        sphere_body->getMotionState()->getWorldTransform(final_trans);
        ASSERT_TRUE(final_trans.getOrigin().y() >= 0.6f && final_trans.getOrigin().y() <= 0.85f);

        // Velocity has decayed to near zero (no endless bouncing or vibrating)
        btVector3 final_vel = sphere_body->getLinearVelocity();
        ASSERT_TRUE(final_vel.length() < 0.2f);
    }

    // =========================================================================
    // Test 10: Complete Application Scene Arena_Terrain Containment & Static Invariance
    // =========================================================================
    {
        Application app;
        ASSERT_TRUE(app.simulation != nullptr);
        ASSERT_TRUE(app.simulation->dynamics_world != nullptr);
        ASSERT_TRUE(app.simulation->terrain != nullptr);

        // Verify Arena_Terrain initial static position & properties
        auto terrain_it = app.simulation->rigid_bodies.find("Arena_Terrain");
        ASSERT_TRUE(terrain_it != app.simulation->rigid_bodies.end());
        if (terrain_it != app.simulation->rigid_bodies.end() && terrain_it->second != nullptr) {
            ASSERT_TRUE(terrain_it->second->isStaticObject());
            btTransform terrain_trans;
            if (terrain_it->second->getMotionState()) {
                terrain_it->second->getMotionState()->getWorldTransform(terrain_trans);
            } else {
                terrain_trans = terrain_it->second->getWorldTransform();
            }
            const float expected_origin_y = 11.25f;
            ASSERT_NEAR(terrain_trans.getOrigin().y(), expected_origin_y, 1e-3f);
            ASSERT_NEAR(0.5f * (app.simulation->terrain->getMinHeight() +
                                app.simulation->terrain->getMaxHeight()),
                        expected_origin_y, 1e-3f);
        }

        // Step 1: Base scene containment across 300 simulation steps (5s at 60Hz)
        app.cascade_mode_active = false;
        int base_tunnel_events = 0;
        for (int frame = 0; frame < 300; ++frame) {
            app.step();
            int tunneling = app.getFloorTunnelingCount();
            if (tunneling > 0) {
                base_tunnel_events += tunneling;
            }
        }
        ASSERT_TRUE(base_tunnel_events == 0);

        // Verify Arena_Terrain remained perfectly immovable at (0, 11.25, 0)
        if (terrain_it != app.simulation->rigid_bodies.end() && terrain_it->second != nullptr) {
            btTransform terrain_trans;
            if (terrain_it->second->getMotionState()) {
                terrain_it->second->getMotionState()->getWorldTransform(terrain_trans);
            } else {
                terrain_trans = terrain_it->second->getWorldTransform();
            }
            ASSERT_NEAR(terrain_trans.getOrigin().y(), 11.25f, 1e-3f);
        }

        // Step 2: Dynamic cascade shower containment across 300 simulation steps
        app.cascade_mode_active = true;
        int cascade_tunnel_events = 0;
        for (int frame = 0; frame < 300; ++frame) {
            app.step();
            int tunneling = app.getFloorTunnelingCount();
            if (tunneling > 0) {
                cascade_tunnel_events += tunneling;
            }
        }
        ASSERT_TRUE(cascade_tunnel_events == 0);

        // Verify Arena_Terrain remained perfectly immovable at (0, 11.25, 0) after 300+300 steps
        if (terrain_it != app.simulation->rigid_bodies.end() && terrain_it->second != nullptr) {
            btTransform terrain_trans;
            if (terrain_it->second->getMotionState()) {
                terrain_it->second->getMotionState()->getWorldTransform(terrain_trans);
            } else {
                terrain_trans = terrain_it->second->getWorldTransform();
            }
            ASSERT_NEAR(terrain_trans.getOrigin().y(), 11.25f, 1e-3f);
        }
    }

    // =========================================================================
    // Test 11: Heightmap Terrain Parity & Ball-Edge Zero-Penetration Invariant
    // =========================================================================
    {
        AssetManager mgr;
        auto terrain = std::make_unique<droidgfxsim::HeightmapTerrain>();
        const bool loaded = terrain->loadFromPNG("terrain_heightmap.png", &mgr, 121, 121, 0.75f, 0.75f, -1.5f, 24.0f);
        ASSERT_TRUE(loaded);

        PhysicsTestSimulation sim;
        sim.dynamics_world->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        scenegraph::TransformNode terrain_node;
        terrain_node.name = "Arena_Terrain";
        btRigidBody* terrain_body = sim.addTerrainNode(&terrain_node, std::move(terrain));
        ASSERT_TRUE(terrain_body != nullptr);
        ASSERT_TRUE(sim.terrain != nullptr);

        // Dense coordinate grid across [-36, +36] covering flat meadow clearing, facing mountain slope,
        // twin curving ravines, side canyon ridges, and both u+v <= 1 and u+v > 1 quad diagonal triangles
        struct SampleCoord { float x; float z; };
        std::vector<SampleCoord> coords;
        for (int ix = -36; ix <= 36; ix += 6) {
            for (int iz = -36; iz <= 36; iz += 6) {
                coords.push_back({static_cast<float>(ix) + 0.18f, static_cast<float>(iz) + 0.15f});
                coords.push_back({static_cast<float>(ix) + 0.55f, static_cast<float>(iz) + 0.60f});
            }
        }
        const SampleCoord extra_coords[] = {
            {0.0f, 18.0f}, {0.0f, 0.0f}, {0.0f, -32.0f}, {35.0f, 0.0f}, {-35.0f, 0.0f},
            {0.0f, -18.0f}, {-6.5f, -16.0f}, {7.0f, -20.0f},
            {-7.5f, -15.0f}, {7.5f, -15.0f}, {16.35f, 4.25f}, {-18.65f, 6.75f}
        };
        for (const auto& c : extra_coords) {
            coords.push_back(c);
        }

        for (const auto& c : coords) {
            const float expected_h = sim.getTerrainHeightAt(c.x, c.z);
            btVector3 ray_from(c.x, 50.0f, c.z);
            btVector3 ray_to(c.x, -20.0f, c.z);
            btCollisionWorld::ClosestRayResultCallback ray_cb(ray_from, ray_to);
            sim.dynamics_world->rayTest(ray_from, ray_to, ray_cb);
            ASSERT_TRUE(ray_cb.hasHit());
            if (ray_cb.hasHit()) {
                ASSERT_NEAR(expected_h, ray_cb.m_hitPointWorld.y(), 2e-3f);
            }
        }

        // Facing hillside landmarks: meadow crate clearing, mountain slope, and side canyon ridges
        ASSERT_NEAR(sim.getTerrainHeightAt(0.0f, 18.0f), 0.25f, 1e-3f);
        ASSERT_TRUE(sim.getTerrainHeightAt(0.0f, -32.0f) > 14.0f);
        ASSERT_TRUE(sim.getTerrainHeightAt(0.0f, 0.0f) > 3.5f);
        ASSERT_TRUE(sim.getTerrainHeightAt(35.0f, 0.0f) > 7.0f);
        ASSERT_TRUE(sim.getTerrainHeightAt(-35.0f, 0.0f) > 7.0f);

        // Drop 3 spheres of radii 0.55f, 0.75f, 1.0f onto the facing hillside slopes slightly above surface
        struct DropSphereSpec {
            const char* name;
            float x;
            float z;
            float radius;
        };
        const DropSphereSpec drop_specs[3] = {
            {"TerrainBall_055", 0.0f, -18.0f, 0.55f},
            {"TerrainBall_075", -6.5f, -16.0f, 0.75f},
            {"TerrainBall_100", 7.0f, -20.0f, 1.0f}
        };
        scenegraph::TransformNode ball_nodes[3];
        float initial_y[3] = {0.0f, 0.0f, 0.0f};
        float initial_z[3] = {0.0f, 0.0f, 0.0f};
        for (int i = 0; i < 3; ++i) {
            const float h0 = sim.getTerrainHeightAt(drop_specs[i].x, drop_specs[i].z);
            initial_y[i] = h0 + drop_specs[i].radius + 0.5f;
            initial_z[i] = drop_specs[i].z;
            ball_nodes[i].name = drop_specs[i].name;
            ball_nodes[i].matrix = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(drop_specs[i].x, initial_y[i], drop_specs[i].z)
            );
            sim.addPhysicsSphereNode(&ball_nodes[i], 2.0f, drop_specs[i].radius);
        }

        for (int step_idx = 0; step_idx < 180; ++step_idx) {
            sim.step();
            for (int i = 0; i < 3; ++i) {
                btRigidBody* body = sim.rigid_bodies[drop_specs[i].name];
                ASSERT_TRUE(body != nullptr);
                if (body != nullptr) {
                    btTransform trans;
                    if (body->getMotionState()) {
                        body->getMotionState()->getWorldTransform(trans);
                    } else {
                        trans = body->getWorldTransform();
                    }
                    const btVector3 pos = trans.getOrigin();
                    const float ground_h = sim.getTerrainHeightAt(
                        static_cast<float>(pos.x()), static_cast<float>(pos.z())
                    );
                    ASSERT_TRUE(static_cast<float>(pos.y()) >= ground_h + drop_specs[i].radius - 0.04f);
                }
            }
        }

        // Verify each sphere actively rolled downhill toward the camera (+Z) and descended in elevation
        for (int i = 0; i < 3; ++i) {
            btRigidBody* body = sim.rigid_bodies[drop_specs[i].name];
            ASSERT_TRUE(body != nullptr);
            if (body != nullptr) {
                btTransform trans;
                if (body->getMotionState()) {
                    body->getMotionState()->getWorldTransform(trans);
                } else {
                    trans = body->getWorldTransform();
                }
                const float final_y = static_cast<float>(trans.getOrigin().y());
                const float final_z = static_cast<float>(trans.getOrigin().z());
                ASSERT_TRUE(final_z > initial_z[i] + 2.5f);
                ASSERT_TRUE(final_y < initial_y[i] - 0.5f);
            }
        }

        // Visual / collision parity check on spawnDynamicDebris sphere archetype
        Application app;
        app.cascade_mode_active = false;
        std::srand(12345);
        bool found_sphere_debris = false;
        for (int attempt = 0; attempt < 36 && !found_sphere_debris; ++attempt) {
            app.spawnDynamicDebris(0.0f, 10.0f, 0.0f, 0.62f, 2.0f);
            if (!app.spawned_debris_nodes.empty()) {
                PhysicsNode* pn = app.spawned_debris_nodes.back();
                if (pn && pn->collision_shape &&
                    pn->collision_shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE) {
                    found_sphere_debris = true;
                    btSphereShape* sphere_shape = static_cast<btSphereShape*>(pn->collision_shape);
                    ASSERT_NEAR(sphere_shape->getRadius(), 0.62f, 1e-4f);
                    ASSERT_TRUE(pn->transform_node != nullptr && !pn->transform_node->children.empty());
                    if (pn->transform_node && !pn->transform_node->children.empty()) {
                        scenegraph::GeometryNode* geom =
                            dynamic_cast<scenegraph::GeometryNode*>(pn->transform_node->children[0]);
                        ASSERT_TRUE(geom != nullptr && !geom->vertex_data.empty());
                        if (geom && !geom->vertex_data.empty()) {
                            float max_vert_radius = 0.0f;
                            for (const auto& v : geom->vertex_data) {
                                float r = std::sqrt(v.position[0] * v.position[0] +
                                                    v.position[1] * v.position[1] +
                                                    v.position[2] * v.position[2]);
                                if (r > max_vert_radius) {
                                    max_vert_radius = r;
                                }
                            }
                            ASSERT_NEAR(max_vert_radius, sphere_shape->getRadius(), 1e-3f);
                        }
                    }
                }
            }
        }
        ASSERT_TRUE(found_sphere_debris);
    }

    // =========================================================================
    // Test 12: Custom 3D Models & Compound Torus Simulation
    // =========================================================================
    {
        Application app;
        ASSERT_TRUE(app.simulation != nullptr);
        ASSERT_TRUE(app.simulation->dynamics_world != nullptr);

        const char* prop_names[5] = {
            "Prop_Crystal_00",
            "Prop_Boulder_00",
            "Prop_Rook_00",
            "Prop_Torus_00",
            "Prop_Glider_00"
        };

        for (const char* name : prop_names) {
            auto it = app.simulation->rigid_bodies.find(name);
            ASSERT_TRUE(it != app.simulation->rigid_bodies.end() && it->second != nullptr);
            if (it != app.simulation->rigid_bodies.end() && it->second != nullptr) {
                ASSERT_TRUE(!it->second->isStaticObject());
            }
        }

        // Verify Prop_Torus_00 uses an 8-sphere btCompoundShape
        auto torus_it = app.simulation->rigid_bodies.find("Prop_Torus_00");
        if (torus_it != app.simulation->rigid_bodies.end() && torus_it->second != nullptr) {
            btCollisionShape* torus_shape = torus_it->second->getCollisionShape();
            ASSERT_TRUE(torus_shape != nullptr);
            ASSERT_TRUE(torus_shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE);
            if (torus_shape && torus_shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE) {
                btCompoundShape* compound = static_cast<btCompoundShape*>(torus_shape);
                ASSERT_TRUE(compound->getNumChildShapes() == 8);
                for (int c = 0; c < compound->getNumChildShapes(); ++c) {
                    ASSERT_TRUE(compound->getChildShape(c) != nullptr);
                    ASSERT_TRUE(compound->getChildShape(c)->getShapeType() == SPHERE_SHAPE_PROXYTYPE);
                }
            }
        }

        // Verify Prop_Crystal_00 and Prop_Glider_00 use ConvexHull shapes
        auto crystal_it = app.simulation->rigid_bodies.find("Prop_Crystal_00");
        if (crystal_it != app.simulation->rigid_bodies.end() && crystal_it->second != nullptr) {
            ASSERT_TRUE(crystal_it->second->getCollisionShape() != nullptr);
            ASSERT_TRUE(crystal_it->second->getCollisionShape()->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE);
        }
        auto glider_it = app.simulation->rigid_bodies.find("Prop_Glider_00");
        if (glider_it != app.simulation->rigid_bodies.end() && glider_it->second != nullptr) {
            ASSERT_TRUE(glider_it->second->getCollisionShape() != nullptr);
            ASSERT_TRUE(glider_it->second->getCollisionShape()->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE);
        }

        int tunnel_events = 0;
        for (int frame = 0; frame < 120; ++frame) {
            app.step();
            tunnel_events += app.getFloorTunnelingCount();
        }
        ASSERT_TRUE(tunnel_events == 0);

        for (const char* name : prop_names) {
            auto it = app.simulation->rigid_bodies.find(name);
            if (it != app.simulation->rigid_bodies.end() && it->second != nullptr) {
                btTransform trans;
                if (it->second->getMotionState()) {
                    it->second->getMotionState()->getWorldTransform(trans);
                } else {
                    trans = it->second->getWorldTransform();
                }
                const btVector3 p = trans.getOrigin();
                const btQuaternion q = trans.getRotation();
                ASSERT_TRUE(std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()));
                ASSERT_TRUE(std::isfinite(q.x()) && std::isfinite(q.y()) &&
                            std::isfinite(q.z()) && std::isfinite(q.w()));
                const float floor_h = app.simulation->getTerrainHeightAt(
                    static_cast<float>(p.x()), static_cast<float>(p.z())
                );
                ASSERT_TRUE(static_cast<float>(p.y()) >= floor_h - 0.5f);
            }
        }
    }

    // =========================================================================
    // Test 13: Application::shootProjectile Regression Invariance (T66)
    // =========================================================================
    {
        Application app;
        ASSERT_TRUE(app.camera != nullptr);
        ASSERT_TRUE(app.simulation != nullptr);
        ASSERT_TRUE(app.simulation->terrain != nullptr);

        app.camera->position = glm::vec3(0.0f, 5.2f, 22.0f);
        app.camera->aim(0.0, 0.0);
        app.camera->update();

        btRigidBody* proj = app.shootProjectile(35.0f);
        ASSERT_TRUE(proj != nullptr);
        if (proj != nullptr) {
            btTransform trans;
            if (proj->getMotionState()) {
                proj->getMotionState()->getWorldTransform(trans);
            } else {
                trans = proj->getWorldTransform();
            }
            const btVector3 pos = trans.getOrigin();
            const float dx = static_cast<float>(pos.x()) - app.camera->position.x;
            const float dy = static_cast<float>(pos.y()) - app.camera->position.y;
            const float dz = static_cast<float>(pos.z()) - app.camera->position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            ASSERT_TRUE(dist <= 3.5f);

            const float ground_h = app.simulation->getTerrainHeightAt(
                static_cast<float>(pos.x()), static_cast<float>(pos.z())
            );
            ASSERT_TRUE(static_cast<float>(pos.y()) >= ground_h);

            const btVector3 vel = proj->getLinearVelocity();
            ASSERT_TRUE(static_cast<float>(vel.length()) > 30.0f);
        }

        int tunnel_events = 0;
        for (int frame = 0; frame < 180; ++frame) {
            app.step();
            tunnel_events += app.getFloorTunnelingCount();
        }
        ASSERT_TRUE(tunnel_events == 0);
        ASSERT_TRUE(app.getFloorTunnelingCount() == 0);
    }

    return failures;
}
