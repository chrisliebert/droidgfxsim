#include "simulation.h"

Simulation::Simulation() {
	///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
	collisionConfiguration = new btDefaultCollisionConfiguration();
	assert(collisionConfiguration);

	///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	dispatcher = new	btCollisionDispatcher(collisionConfiguration);
	assert(dispatcher);

	///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
	overlappingPairCache = new btDbvtBroadphase();
	assert(overlappingPairCache);

	///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	solver = new btSequentialImpulseConstraintSolver;
	assert(solver);

	dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,overlappingPairCache,solver,collisionConfiguration);
	assert(dynamicsWorld);

	dynamicsWorld->setGravity(btVector3(0,-10,0));
}

Simulation::~Simulation() {
	//cleanup in the reverse order of creation/initialization

	///-----cleanup_start-----

	//remove the rigidbodies from the dynamics world and delete them
	for (int i=dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
	{
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
		{
			delete body->getMotionState();
		}
		dynamicsWorld->removeCollisionObject( obj );
		delete obj;
	}

	//delete collision shapes
	for (int j=0;j<collisionShapes.size();j++)
	{
		btCollisionShape* shape = collisionShapes[j];
		collisionShapes[j] = 0;
		delete shape;
	}

	//delete dynamics world
	delete dynamicsWorld;

	//delete solver
	delete solver;

	//delete broadphase
	delete overlappingPairCache;

	//delete dispatcher
	delete dispatcher;

	delete collisionConfiguration;

	//next line is optional: it will be cleared by the destructor when the array goes out of scope
	collisionShapes.clear();
    physics_nodes.clear();
}

void Simulation::addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass, float offset_x, float offset_y, float offset_z) {
	assert(collision_shape);
	assert(trans_node);
	collisionShapes.push_back(collision_shape);

	/// Create Dynamic Objects
	btTransform start_transform;
	start_transform.setIdentity();

	// rigid body is dynamic if mass is non zero, otherwise it is static
	bool is_dynamic = (mass != 0.f);

	btVector3 local_inertia(0,0,0);
	if (is_dynamic) {
		collision_shape->calculateLocalInertia(mass, local_inertia);
	}

	glm::vec4 translation = trans_node->matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	start_transform.setOrigin(btVector3(translation.x + offset_x, translation.y + offset_y, translation.z + offset_z));

	btDefaultMotionState* motion_state = new btDefaultMotionState(start_transform);
	assert(motion_state);
	btRigidBody::btRigidBodyConstructionInfo rb_info(mass,motion_state, collision_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rb_info);
	assert(body);
	dynamicsWorld->addRigidBody(body);

	PhysicsNode physics_node;
	physics_node.collision_shape = collision_shape;
	physics_node.body = body;
	physics_node.transform_node = trans_node;
	physics_node.mass = btScalar(mass);
	int index = (int)(collisionShapes.size() - 1);
	physics_nodes[index] = physics_node;
}

void Simulation::addPhysicsNode(TransformNode* trans_node, btCollisionShape* collision_shape, float mass) {
	addPhysicsNode(trans_node, collision_shape, mass, 0.f, 0.f, 0.f);
}

void Simulation::addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length, float offset_x, float offset_y, float offset_z) {
	btCollisionShape* collision_shape = new btBoxShape(btVector3(width, height, length));
	assert(collision_shape);
	addPhysicsNode(trans_node, collision_shape, mass, offset_x, offset_y, offset_z);
}

void Simulation::addPhysicsBoxNode(TransformNode* trans_node, float mass, float width, float height, float length) {
	addPhysicsBoxNode(trans_node, mass, width, height, length, 0.f, 0.f, 0.f);
}

void Simulation::step() {
	dynamicsWorld->stepSimulation(1.f/60.f,10);
}
