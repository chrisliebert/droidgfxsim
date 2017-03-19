#ifndef _SIMULATION_H_
#define _SIMULATION_H_

#include <cassert>

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
	btDefaultCollisionConfiguration* collisionConfiguration;
	btCollisionDispatcher* dispatcher;
	btBroadphaseInterface* overlappingPairCache;
	btSequentialImpulseConstraintSolver* solver;
public:
	btDiscreteDynamicsWorld* dynamicsWorld;
	btAlignedObjectArray<btCollisionShape*> collisionShapes;
	std::map<int, PhysicsNode> physics_nodes;

    void addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass, float offset_x, float offset_y, float offset_z);
    void addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass);
    void addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length, float offset_x, float offset_y, float offset_z);
    void addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length);

	Simulation();
	~Simulation();
	void step();
};

#endif
