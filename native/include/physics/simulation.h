// Copyright (C) 2017-2026 Chris Liebert

#ifndef _SIMULATION_H_
#define _SIMULATION_H_

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rapidxml.hpp"
#include "common/log.h"
#include "btBulletDynamicsCommon.h"
#include "graphics/scene_graph.h"
#include "physics/terrain.h"

using scenegraph::TransformNode;

typedef struct PhysicsNode {
	btCollisionShape* collision_shape;
	btRigidBody* body;
	btScalar mass;
	TransformNode* transform_node;
} PhysicsNode;

class Simulation {
protected:
	btDefaultCollisionConfiguration* collision_configuration;
	btCollisionDispatcher* dispatcher;
	btBroadphaseInterface* overlapping_pair_cache;
	btSequentialImpulseConstraintSolver* solver;
	std::vector<PhysicsNode*> allocated_physics_nodes;

    void addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass, float offset_x, float offset_y, float offset_z);
    void addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass);
    void addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length, float offset_x, float offset_y, float offset_z);
    void addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length);
	void addPhysicsConvexHullNode(TransformNode* trans_node, float mass);
public:
	std::unique_ptr<droidgfxsim::HeightmapTerrain> terrain;
	btDiscreteDynamicsWorld* dynamics_world;
	btAlignedObjectArray<btCollisionShape*> collision_shapes;
	std::map<std::string, btBoxShape*> box_shapes;
	std::map<std::string, btSphereShape*> sphere_shapes;
	std::map<std::string, btCylinderShape*> cylinder_shapes;
	std::map<std::string, btConvexHullShape*> convex_hull_shapes;
	std::map<std::string, btCompoundShape*> compound_shapes;
	std::map<std::string, btRigidBody*> rigid_bodies;
	std::map<int, PhysicsNode*> collision_node_index;

    Simulation();
	~Simulation();
	void addPhysicsSphereNode(TransformNode* trans_node, float mass, float radius, float offset_x = 0.f, float offset_y = 0.f, float offset_z = 0.f);
	void addPhysicsCylinderNode(TransformNode* trans_node, float mass, float radius, float height, float offset_x = 0.f, float offset_y = 0.f, float offset_z = 0.f);
	void addPhysicsCompoundNode(TransformNode* trans_node, btCompoundShape* compound_shape, float mass);
	btCompoundShape* createTorusCompoundShape(float major_radius, float minor_radius, int segments = 8, int axis = 0);
	btRigidBody* addTerrainNode(TransformNode* trans_node, std::unique_ptr<droidgfxsim::HeightmapTerrain> heightfield);
	float getTerrainHeightAt(float x, float z) const;
	void applyForce(const std::string& rigid_body_name, const btVector3& force, const btVector3& rel_pos);
	void parseXMLNode(rapidxml::xml_node<>* my_xml_node, scenegraph::Node* scene_node);
	void step();
};

#endif // _SIMULATION_H_
