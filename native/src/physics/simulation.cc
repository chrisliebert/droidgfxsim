// Copyright (C) 2017-2026 Chris Liebert

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
	// Free all allocated PhysicsNode instances (fixes memory leak)
	for (PhysicsNode* p : allocated_physics_nodes) {
		delete p;
	}
	allocated_physics_nodes.clear();
	collision_node_index.clear();

	rigid_bodies.clear();
	box_shapes.clear();
	sphere_shapes.clear();
	cylinder_shapes.clear();
	convex_hull_shapes.clear();

	if (dynamics_world) {
		for (int i = dynamics_world->getNumCollisionObjects() - 1; i >= 0; i--) {
			btCollisionObject* obj = dynamics_world->getCollisionObjectArray()[i];
			btRigidBody* body = btRigidBody::upcast(obj);
			if (body && body->getMotionState()) {
				delete body->getMotionState();
			}
			dynamics_world->removeCollisionObject(obj);
			delete obj;
		}
	}

	// Delete collision shapes
	for (int j = 0; j < collision_shapes.size(); j++) {
		btCollisionShape* shape = collision_shapes[j];
		collision_shapes[j] = 0;
		delete shape;
	}
	collision_shapes.clear();

	delete dynamics_world;
	dynamics_world = nullptr;
	delete solver;
	solver = nullptr;
	delete overlapping_pair_cache;
	overlapping_pair_cache = nullptr;
	delete dispatcher;
	dispatcher = nullptr;
	delete collision_configuration;
	collision_configuration = nullptr;
}

void Simulation::parseXMLNode(rapidxml::xml_node<>* my_xml_node, scenegraph::Node* scenegraph_root) {
	for (rapidxml::xml_node<>* node = my_xml_node; node; node = node->next_sibling()) {
		char* c_name = node->name();
		if (!c_name || strlen(c_name) == 0) continue;
		std::string name(c_name);
		if (0 == std::string("PhysicsNode").compare(name)) {
			float mass = 0.f;
			float offset_x = 0.f;
			float offset_y = 0.f;
			float offset_z = 0.f;
			float width = 0.f;
			float height = 0.f;
			float length = 0.f;
			float radius = 0.f;
			std::string object_name("");
			for (rapidxml::xml_attribute<> *attr = node->first_attribute();
					attr; attr = attr->next_attribute()) {
				if (0 == std::string("mass").compare(attr->name())) {
					mass = (float) atof(attr->value());
				} else if (0 == std::string("name").compare(attr->name())) {
					object_name = std::string(attr->value());
				}
			}
			for (rapidxml::xml_node<>* child = node->first_node(); child;
					child = child->next_sibling()) {
				if (0 == std::string("CollisionShape").compare(child->name())) {
					std::string shape_type = child->value() ? child->value() : "";
					size_t first = shape_type.find_first_not_of(" \t\r\n");
					size_t last = shape_type.find_last_not_of(" \t\r\n");
					if (first != std::string::npos && last != std::string::npos) {
						shape_type = shape_type.substr(first, last - first + 1);
					}

					if (0 == std::string("Box").compare(shape_type)) {
						for (rapidxml::xml_attribute<> *col_attr =
								child->first_attribute(); col_attr; col_attr =
								col_attr->next_attribute()) {
							if (0 == std::string("width").compare(col_attr->name())) {
								width = (float) atof(col_attr->value());
							} else if (0 == std::string("height").compare(col_attr->name())) {
								height = (float) atof(col_attr->value());
							} else if (0 == std::string("length").compare(col_attr->name())) {
								length = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_x").compare(col_attr->name())) {
								offset_x = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_y").compare(col_attr->name())) {
								offset_y = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_z").compare(col_attr->name())) {
								offset_z = (float) atof(col_attr->value());
							}
						}
						TransformNode* trans_node = scenegraph::find_transform_node(object_name, scenegraph_root);
						if (trans_node) {
							addPhysicsBoxNode(trans_node, mass, width, height,
									length, offset_x, offset_y, offset_z);
							break;
						} else {
							LOGI("%s was referenced in configuration, but not loaded", object_name.c_str());
						}
					} else if (0 == std::string("Sphere").compare(shape_type)) {
						for (rapidxml::xml_attribute<> *col_attr =
								child->first_attribute(); col_attr; col_attr =
								col_attr->next_attribute()) {
							if (0 == std::string("radius").compare(col_attr->name())) {
								radius = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_x").compare(col_attr->name())) {
								offset_x = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_y").compare(col_attr->name())) {
								offset_y = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_z").compare(col_attr->name())) {
								offset_z = (float) atof(col_attr->value());
							}
						}
						TransformNode* trans_node = scenegraph::find_transform_node(object_name, scenegraph_root);
						if (trans_node) {
							addPhysicsSphereNode(trans_node, mass, radius, offset_x, offset_y, offset_z);
							break;
						} else {
							LOGI("%s was referenced in configuration, but not loaded", object_name.c_str());
						}
					} else if (0 == std::string("Cylinder").compare(shape_type)) {
						for (rapidxml::xml_attribute<> *col_attr =
								child->first_attribute(); col_attr; col_attr =
								col_attr->next_attribute()) {
							if (0 == std::string("radius").compare(col_attr->name())) {
								radius = (float) atof(col_attr->value());
							} else if (0 == std::string("height").compare(col_attr->name())) {
								height = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_x").compare(col_attr->name())) {
								offset_x = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_y").compare(col_attr->name())) {
								offset_y = (float) atof(col_attr->value());
							} else if (0 == std::string("offset_z").compare(col_attr->name())) {
								offset_z = (float) atof(col_attr->value());
							}
						}
						TransformNode* trans_node = scenegraph::find_transform_node(object_name, scenegraph_root);
						if (trans_node) {
							addPhysicsCylinderNode(trans_node, mass, radius, height, offset_x, offset_y, offset_z);
							break;
						} else {
							LOGI("%s was referenced in configuration, but not loaded", object_name.c_str());
						}
					} else if (0 == std::string("ConvexHull").compare(shape_type)) {
						TransformNode* trans_node = scenegraph::find_transform_node(object_name, scenegraph_root);
						if (trans_node) {
							addPhysicsConvexHullNode(trans_node, mass);
							break;
						} else {
							LOGI("%s was referenced in configuration, but not loaded", object_name.c_str());
						}
					} else {
						LOGI("WARNING, only Box, Sphere, Cylinder, and ConvexHull collision shapes are implemented %s.", shape_type.c_str());
					}
				}
			}
		}

		if (node->first_node()) {
			parseXMLNode(node->first_node(), scenegraph_root);
		}
	}
}

void Simulation::addPhysicsNode(TransformNode* trans_node,
		btCollisionShape* collision_shape, float mass, float offset_x,
		float offset_y, float offset_z) {
	assert(collision_shape);
	assert(trans_node);
	collision_shapes.push_back(collision_shape);

	btTransform start_transform;
	start_transform.setIdentity();

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

	btDefaultMotionState* motion_state = new btDefaultMotionState(start_transform);
	assert(motion_state);
	btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state,
			collision_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rb_info);
	assert(body);
	rigid_bodies.insert(std::make_pair(trans_node->name, body));
	dynamics_world->addRigidBody(body);

	PhysicsNode* physics_node = new PhysicsNode;
	assert(physics_node);
	physics_node->collision_shape = collision_shape;
	physics_node->body = body;
	physics_node->transform_node = trans_node;
	physics_node->mass = btScalar(mass);
	int index = dynamics_world->getNumCollisionObjects() - 1;
	collision_node_index.insert(std::make_pair(index, physics_node));
	allocated_physics_nodes.push_back(physics_node);
}

void Simulation::addPhysicsNode(TransformNode* trans_node,
		btCollisionShape* collision_shape, float mass) {
	addPhysicsNode(trans_node, collision_shape, mass, 0.f, 0.f, 0.f);
}

void Simulation::addPhysicsBoxNode(TransformNode* trans_node, float mass,
		float width, float height, float length, float offset_x, float offset_y,
		float offset_z) {
	btBoxShape* collision_shape = new btBoxShape(btVector3(width, height, length));
	assert(collision_shape);
	box_shapes.insert(std::make_pair(trans_node->name, collision_shape));
	addPhysicsNode(trans_node, collision_shape, mass, offset_x, offset_y, offset_z);
	auto it = rigid_bodies.find(trans_node->name);
	if (it != rigid_bodies.end() && it->second) {
		if (trans_node->name == "Arena_Floor" || trans_node->name.find("Ramp") != std::string::npos || trans_node->name.find("Platform") != std::string::npos) {
			it->second->setRestitution(0.75f);
		} else {
			it->second->setRestitution(0.65f);
			it->second->setFriction(0.4f);
		}
	}
}

void Simulation::addPhysicsBoxNode(TransformNode* trans_node, float mass,
		float width, float height, float length) {
	addPhysicsBoxNode(trans_node, mass, width, height, length, 0.f, 0.f, 0.f);
}

void Simulation::addPhysicsSphereNode(TransformNode* trans_node, float mass,
		float radius, float offset_x, float offset_y, float offset_z) {
	btSphereShape* collision_shape = new btSphereShape(radius);
	assert(collision_shape);
	sphere_shapes.insert(std::make_pair(trans_node->name, collision_shape));
	addPhysicsNode(trans_node, collision_shape, mass, offset_x, offset_y, offset_z);
	auto it = rigid_bodies.find(trans_node->name);
	if (it != rigid_bodies.end() && it->second) {
		it->second->setRestitution(0.82f);
		it->second->setRollingFriction(0.015f);
		it->second->setFriction(0.3f);
	}
}

void Simulation::addPhysicsCylinderNode(TransformNode* trans_node, float mass,
		float radius, float height, float offset_x, float offset_y, float offset_z) {
	btCylinderShape* collision_shape = new btCylinderShape(btVector3(radius, height * 0.5f, radius));
	assert(collision_shape);
	cylinder_shapes.insert(std::make_pair(trans_node->name, collision_shape));
	addPhysicsNode(trans_node, collision_shape, mass, offset_x, offset_y, offset_z);
	auto it = rigid_bodies.find(trans_node->name);
	if (it != rigid_bodies.end() && it->second) {
		it->second->setRestitution(0.70f);
	}
}

void populateConvexHullShapeFromNode(scenegraph::Node* root, btConvexHullShape* convex_hull, glm::mat4 currentMatrix) {
	if (root == nullptr) return;
	if (root->type == scenegraph::NodeType::Geometry) {
		scenegraph::GeometryNode* geometry_node = (scenegraph::GeometryNode*) root;
		for (GLuint i : geometry_node->index_data) {
			if (i < geometry_node->vertex_data.size()) {
				const scenegraph::Vertex& v = geometry_node->vertex_data[i];
				// Column-vector multiplication: currentMatrix * v
				glm::vec4 v_trans = currentMatrix * glm::vec4(v.position[0], v.position[1], v.position[2], 1.0f);
				convex_hull->addPoint(btVector3(v_trans.x, v_trans.y, v_trans.z));
			}
		}
	} else if (root->type == scenegraph::NodeType::Transform) {
		scenegraph::TransformNode* transform_node = (scenegraph::TransformNode*) root;
		currentMatrix = currentMatrix * transform_node->matrix;
	}
	for (scenegraph::Node* child : root->children) {
		populateConvexHullShapeFromNode(child, convex_hull, currentMatrix);
	}
}

void Simulation::addPhysicsConvexHullNode(TransformNode* trans_node, float mass) {
	btConvexHullShape* convex_hull_shape = new btConvexHullShape();
	assert(convex_hull_shape);
	convex_hull_shapes.insert(std::make_pair(trans_node->name, convex_hull_shape));
	glm::mat4 identity(1.f);
	for (scenegraph::Node* child : trans_node->children) {
		populateConvexHullShapeFromNode(child, convex_hull_shape, identity);
	}
	addPhysicsNode(trans_node, convex_hull_shape, mass);
	auto it = rigid_bodies.find(trans_node->name);
	if (it != rigid_bodies.end() && it->second) {
		it->second->setRestitution(0.75f);
	}
}

void Simulation::step() {
	if (dynamics_world) {
		dynamics_world->stepSimulation(1.f / 60.f, 10);
	}
}

void Simulation::applyForce(const std::string& rigid_body_name, const btVector3& force, const btVector3& rel_pos) {
	std::map<std::string, btRigidBody*>::iterator itr = rigid_bodies.find(rigid_body_name);
	if (itr != rigid_bodies.end()) {
		itr->second->applyForce(force, rel_pos);
	} else {
		LOGI("Unable to find rigid body called %s, cannot apply force", rigid_body_name.c_str());
	}
}
