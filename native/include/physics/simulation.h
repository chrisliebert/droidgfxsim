// Copyright (C) 2017 Chris Liebert

#ifndef _SIMULATION_H_
#define _SIMULATION_H_

#include <cassert>

#include "rapidxml.hpp"
#include "common/log.h"
#include "btBulletDynamicsCommon.h"
#include "graphics/scene_graph.h"

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

    void addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass, float offset_x, float offset_y, float offset_z);
    void addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass);
    void addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length, float offset_x, float offset_y, float offset_z);
    void addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length);
	void addPhysicsConvexHullNode(TransformNode* trans_node, float mass);
public:
	btDiscreteDynamicsWorld* dynamics_world;
	btAlignedObjectArray<btCollisionShape*> collision_shapes;
	std::map<std::string, btBoxShape*> box_shapes;
	std::map<std::string, btConvexHullShape*> convex_hull_shapes;
	std::map<std::string, btRigidBody*> rigid_bodies;
	std::map<int, PhysicsNode*> collision_node_index;

    Simulation();
	~Simulation();
	void applyForce(const std::string& rigid_body_name, const btVector3& force, const btVector3& rel_pos);
	void parseXMLNode(rapidxml::xml_node<>* my_xml_node, scenegraph::Node* scene_node);
	void step();
};

#endif
