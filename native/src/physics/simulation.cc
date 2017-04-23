// Copyright (C) 2017 Chris Liebert

#include <cmath>
#include "simulation.h"

Simulation::Simulation() {
	collision_configuration = new btDefaultCollisionConfiguration();
	assert(collision_configuration);
	dispatcher = new btCollisionDispatcher(collision_configuration);
	assert(dispatcher);
	overlapping_pair_cache = new btDbvtBroadphase();
	assert(overlapping_pair_cache);
	solver = new btSequentialImpulseConstraintSolver;
	assert(solver);
	dynamics_world = new btDiscreteDynamicsWorld(dispatcher,
			overlapping_pair_cache, solver, collision_configuration);
	assert(dynamics_world);
	dynamics_world->setGravity(btVector3(0, -10, 0));
}

Simulation::~Simulation() {
	//cleanup in the reverse order of creation/initialization
	//remove the rigid bodies from the dynamics world and delete them
	for (int i = dynamics_world->getNumCollisionObjects() - 1; i >= 0; i--) {
		btCollisionObject* obj = dynamics_world->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState()) {
			delete body->getMotionState();
		}
		dynamics_world->removeCollisionObject(obj);
		delete obj;
	}

	//delete collision shapes
	for (int j = 0; j < collision_shapes.size(); j++) {
		btCollisionShape* shape = collision_shapes[j];
		collision_shapes[j] = 0;
		delete shape;
	}

	//delete dynamics world
	delete dynamics_world;

	//delete solver
	delete solver;

	//delete broadphase
	delete overlapping_pair_cache;

	//delete dispatcher
	delete dispatcher;

	delete collision_configuration;

	//next line is optional: it will be cleared by the destructor when the array goes out of scope
	collision_shapes.clear();
	physics_nodes.clear();
}

void Simulation::parseXMLNode(rapidxml::xml_node<>* my_xml_node, scenegraph::Node* scenegraph_root) {
	if (!my_xml_node)
		return;
	char* c_name = my_xml_node->name();
	if (0 == strlen(c_name))
		return;
	std::string name(c_name);
	if (0 == std::string("PhysicsNode").compare(name)) {
		float mass = 0.f;
		float offset_x = 0.f;
		float offset_y = 0.f;
		float offset_z = 0.f;
		float width = 0.f;
		float height = 0.f;
		float length = 0.f;
		std::string object_name("");
		for (rapidxml::xml_attribute<> *attr = my_xml_node->first_attribute();
				attr; attr = attr->next_attribute()) {
			if (0 == std::string("mass").compare(attr->name())) {
				mass = atof(attr->value());
			} else if (0 == std::string("name").compare(attr->name())) {
				object_name = std::string(attr->value());
			}
		}
		for (rapidxml::xml_node<>* child = my_xml_node->first_node(); child;
				child = child->next_sibling()) {
			if (0 == std::string("CollisionShape").compare(child->name())) {
				if (0 == std::string("Box").compare(child->value())) {
					for (rapidxml::xml_attribute<> *col_attr =
							child->first_attribute(); col_attr; col_attr =
							col_attr->next_attribute()) {
						if (0
								== std::string("width").compare(
										col_attr->name())) {
							width = atof(col_attr->value());
						} else if (0
								== std::string("height").compare(
										col_attr->name())) {
							height = atof(col_attr->value());
						} else if (0
								== std::string("length").compare(
										col_attr->name())) {
							length = atof(col_attr->value());
						} else if (0
								== std::string("offset_x").compare(
										col_attr->name())) {
							offset_x = atof(col_attr->value());
						} else if (0
								== std::string("offset_y").compare(
										col_attr->name())) {
							offset_y = atof(col_attr->value());
						} else if (0
								== std::string("offset_z").compare(
										col_attr->name())) {
							offset_z = atof(col_attr->value());
						}
					}
					TransformNode* trans_node = scenegraph::find_transform_node(object_name, scenegraph_root);
					if (trans_node) {
						addPhysicsBoxNode(trans_node, mass, width, height,
								length, offset_x, offset_y, offset_z);
						break;
					} else {
						LOGE(
								"%s was referenced in configuration, but not loaded",
								object_name.c_str());
					}
				} else {
					LOGE("WARNING, only box collision shape is implemented %s.",
							child->value());
				}
			}
		}
	}

	// Recursively parse children and siblings
	parseXMLNode(my_xml_node->first_node(), scenegraph_root);
	parseXMLNode(my_xml_node->next_sibling(), scenegraph_root);
}

void Simulation::addPhysicsNode(TransformNode* trans_node,
		btCollisionShape* collision_shape, float mass, float offset_x,
		float offset_y, float offset_z) {
	assert(collision_shape);
	assert(trans_node);
	collision_shapes.push_back(collision_shape);

	/// Create Dynamic Objects
	btTransform start_transform;
	start_transform.setIdentity();

	// rigid body is dynamic if mass is non zero, otherwise it is static
	bool is_dynamic = (mass != 0.f);

	btVector3 local_inertia(0, 0, 0);
	if (is_dynamic) {
		collision_shape->calculateLocalInertia(mass, local_inertia);
	}

	glm::vec4 translation = trans_node->matrix
			* glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	start_transform.setOrigin(
			btVector3(translation.x + offset_x, translation.y + offset_y,
					translation.z + offset_z));

	btDefaultMotionState* motion_state = new btDefaultMotionState(
			start_transform);
	assert(motion_state);
	btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state,
			collision_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rb_info);
	assert(body);
	dynamics_world->addRigidBody(body);

	PhysicsNode physics_node;
	physics_node.collision_shape = collision_shape;
	physics_node.body = body;
	physics_node.transform_node = trans_node;
	physics_node.mass = btScalar(mass);
	int index = (int) (collision_shapes.size() - 1);
	physics_nodes[index] = physics_node;
}

void Simulation::addPhysicsNode(TransformNode* trans_node,
		btCollisionShape* collision_shape, float mass) {
	addPhysicsNode(trans_node, collision_shape, mass, 0.f, 0.f, 0.f);
}

void Simulation::addPhysicsBoxNode(TransformNode* trans_node, float mass,
		float width, float height, float length, float offset_x, float offset_y,
		float offset_z) {
	btCollisionShape* collision_shape = new btBoxShape(
			btVector3(width, height, length));
	assert(collision_shape);
	addPhysicsNode(trans_node, collision_shape, mass, offset_x, offset_y,
			offset_z);
}

void Simulation::addPhysicsBoxNode(TransformNode* trans_node, float mass,
		float width, float height, float length) {
	addPhysicsBoxNode(trans_node, mass, width, height, length, 0.f, 0.f, 0.f);
}

void Simulation::step() {
	dynamics_world->stepSimulation(1.f / 60.f, 10);
}
