#include "test_helpers.h"
#include "physics/simulation.h"
#include <vector>

// Derived test harness to expose protected Simulation methods and provide
// shape helpers (addPhysicsSphereNode, addPhysicsCylinderNode)
class PhysicsTestSimulation : public Simulation {
public:
    using Simulation::addPhysicsNode;
    using Simulation::addPhysicsBoxNode;
    using Simulation::addPhysicsConvexHullNode;

    void addPhysicsSphereNode(TransformNode* trans_node, float mass, float radius,
                              float offset_x = 0.0f, float offset_y = 0.0f, float offset_z = 0.0f) {
        btSphereShape* sphere_shape = new btSphereShape(radius);
        addPhysicsNode(trans_node, sphere_shape, mass, offset_x, offset_y, offset_z);
    }

    void addPhysicsCylinderNode(TransformNode* trans_node, float mass, float radius, float height,
                                float offset_x = 0.0f, float offset_y = 0.0f, float offset_z = 0.0f) {
        // Bullet btCylinderShape has its default symmetry axis along Y
        btCylinderShape* cylinder_shape = new btCylinderShape(btVector3(radius, height * 0.5f, radius));
        addPhysicsNode(trans_node, cylinder_shape, mass, offset_x, offset_y, offset_z);
    }
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

    return failures;
}
