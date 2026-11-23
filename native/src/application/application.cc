// Copyright (C) 2017-2026 Chris Liebert

#include <memory>
#include <sstream>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>

#include "application/application.h"
#include "common/log.h"
#include "physics/raycast_picker.h"
#include "physics/projectile_spawner.h"
#include "graphics/scene_presets.h"
#if !defined(__ANDROID__)
#include "graphics/bullet_debug_drawer.h"
#endif

#define XML_FILENAME "data.xml"

scenegraph::Node* Application::loadResources() {
	return loadXML(XML_FILENAME);
}

#if defined(__ANDROID__)
Application::Application(AAssetManager* android_asset_manager) {
    this->asset_manager = new AssetManager(android_asset_manager);
#else
Application::Application() {
    this->asset_manager = new AssetManager();
#endif
	assert(this->asset_manager);
    config_file_contents = nullptr;
    camera = nullptr;
    scenegraph_root = nullptr;
    picker = nullptr;
    spawner = nullptr;
    debug_drawer = nullptr;
    current_preset_index = -1;
    cascade_mode_active = true;
    cascade_timer = 0.0f;
    cascade_interval = 0.25f;
    simulation = new Simulation();
	assert(simulation);
	scenegraph_root = loadResources();
	init();
    if(config_file_contents) {
        delete [] config_file_contents;
        config_file_contents = nullptr;
    }
}

Application::~Application() {
	if (simulation && simulation->dynamics_world) {
		simulation->dynamics_world->setDebugDrawer(nullptr);
	}
	if (picker) {
		delete picker;
		picker = nullptr;
	}
	if (spawner) {
		delete spawner;
		spawner = nullptr;
	}
#if !defined(__ANDROID__)
	if (debug_drawer) {
		delete debug_drawer;
		debug_drawer = nullptr;
	}
#endif
	for (PhysicsNode* p : spawned_debris_nodes) {
		delete p;
	}
	spawned_debris_nodes.clear();
	if (simulation) {
		delete simulation;
		simulation = nullptr;
	}
	if (scenegraph_root) {
		scenegraph::destroy(scenegraph_root);
		scenegraph_root = nullptr;
	}
	for (auto& pair : images) {
		if (pair.second) {
			delete pair.second;
			pair.second = nullptr;
		}
	}
	images.clear();
	if (camera) {
		delete camera;
		camera = nullptr;
	}
	if(asset_manager) {
		delete asset_manager;
		asset_manager = nullptr;
	}
}

scenegraph::Node* Application::parseXML(rapidxml::xml_document<>& doc) {
	scenegraph::Node* scene_node = new scenegraph::Node();
	assert(scene_node);
	scene_node->name = std::string(doc.name() ? doc.name() : "root") + std::string(" node");
	parseXMLNode(doc.first_node(), scene_node);
	simulation->parseXMLNode(doc.first_node(), scene_node);
	return scene_node;
}

void Application::parseXMLNode(rapidxml::xml_node<>* my_xml_node,
		scenegraph::Node* scene_node) {
	for (rapidxml::xml_node<>* node = my_xml_node; node; node = node->next_sibling()) {
		std::string name(node->name() ? node->name() : "");
		if (0 == std::string("WavefrontFile").compare(name)) {
			for (rapidxml::xml_attribute<> *attr = node->first_attribute();
					attr; attr = attr->next_attribute()) {
				if (0 == std::string("filename").compare(attr->name())) {
					WavefrontSceneGraphFactory factory;
					bool status = factory.addWavefront(attr->value(), glm::mat4(1.f), asset_manager);
					if (status) {
						for (const auto& s : factory.textures) {
							if(images.find(s) == images.end()) {
								Image* t = new Image(s.c_str(), asset_manager);
								assert(t);
								images.insert(std::make_pair(s, t));
							}
						}
						scenegraph::Node* wf = factory.build();
						if (wf) {
							scene_node->children.push_back(wf);
						}
					}
				}
			}
		}

		if (node->first_node()) {
			parseXMLNode(node->first_node(), scene_node);
		}
	}
}

scenegraph::Node* Application::loadXML(const char* xml_filename) {
	auto doc = std::make_unique<rapidxml::xml_document<>>();
    config_file_contents = asset_manager->loadTextChars(xml_filename);
    if(config_file_contents == nullptr) {
    	LOGE("Failed to read XML file %s", xml_filename);
    	return nullptr;
    }
	doc->parse<0>(config_file_contents);
	return parseXML(*doc);
}

void Application::init() {
	if (!camera) {
		camera = new Camera();
	}
	assert(camera);
	camera->position.y = 4.0;
	camera->moveBackward(28.0);
	camera->update();

	if (simulation && simulation->dynamics_world) {
#if !defined(__ANDROID__)
		if (!debug_drawer) {
			debug_drawer = new BulletDebugDrawer();
		}
		simulation->dynamics_world->setDebugDrawer(debug_drawer);
#endif
		if (!picker) {
			picker = new droidgfxsim::RaycastPicker(simulation->dynamics_world);
		} else {
			picker->setDynamicsWorld(simulation->dynamics_world);
		}
		if (!spawner) {
			spawner = new droidgfxsim::ProjectileSpawner(simulation->dynamics_world);
		} else {
			spawner->setDynamicsWorld(simulation->dynamics_world);
		}
	}
}

void Application::resize(int width, int height) {
	if (!camera) return;
	float aspect = (height > 0) ? ((float)width / (float)height) : 1.0f;
	camera->projection_matrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
	camera->update();
}

void Application::update(float dt) {
	if (cascade_mode_active) {
		cascade_timer += dt;
		if (cascade_timer >= cascade_interval) {
			cascade_timer = 0.0f;
			// Wide bombardment across entire arena: X in [-8.5, 8.5], Y in [28.0, 44.0], Z in [-8.0, 16.0]
			float rx = -8.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 17.0f));
			float ry = 30.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 14.0f));
			float rz = -8.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 24.0f));
			float rradius = 0.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.6f));
			float rmass = 1.8f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 2.0f));
			spawnDynamicDebris(rx, ry, rz, rradius, rmass);
		}
	}
}

void Application::spawnDynamicDebris(float x, float y, float z, float radius, float mass) {
	if (!simulation || !simulation->dynamics_world)
		return;

	// Pick between dynamic bouncing sphere or box
	bool is_sphere = (rand() % 2 == 0);
	btCollisionShape* shape = nullptr;
	if (is_sphere) {
		shape = new btSphereShape(radius);
	} else {
		shape = new btBoxShape(btVector3(radius * 0.8f, radius * 0.8f, radius * 0.8f));
	}
	simulation->collision_shapes.push_back(shape);

	btTransform start_transform;
	start_transform.setIdentity();
	start_transform.setOrigin(btVector3(x, y, z));

	btDefaultMotionState* motion_state = new btDefaultMotionState(start_transform);
	btVector3 inertia(0.0f, 0.0f, 0.0f);
	shape->calculateLocalInertia(mass, inertia);

	btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, shape, inertia);
	if (is_sphere) {
		rb_info.m_restitution = 0.82f;
		rb_info.m_friction = 0.3f;
		rb_info.m_rollingFriction = 0.015f;
	} else {
		rb_info.m_restitution = 0.65f;
		rb_info.m_friction = 0.4f;
	}

	btRigidBody* body = new btRigidBody(rb_info);

	// Random downward / forward velocity and angular spin
	float vx = -3.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 6.0f));
	float vy = -9.0f - static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 7.0f));
	float vz = -2.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 8.0f));
	body->setLinearVelocity(btVector3(vx, vy, vz));

	float wx = -6.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 12.0f));
	float wy = -6.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 12.0f));
	float wz = -6.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 12.0f));
	body->setAngularVelocity(btVector3(wx, wy, wz));

	body->setCcdMotionThreshold(0.2f);
	body->setCcdSweptSphereRadius(0.2f);

	simulation->dynamics_world->addRigidBody(body);

	if (scenegraph_root) {
		TransformNode* template_trans = nullptr;
		if (is_sphere) {
			template_trans = scenegraph::find_transform_node("Ball_00", scenegraph_root);
		} else {
			template_trans = scenegraph::find_transform_node("Crate_00", scenegraph_root);
		}

		if (template_trans && !template_trans->children.empty()) {
			GeometryNode* src_geom = dynamic_cast<GeometryNode*>(template_trans->children[0]);
			if (src_geom) {
				GeometryNode* geom_node = new GeometryNode();
				geom_node->name = "DynamicDebrisGeom_" + std::to_string(simulation->dynamics_world->getNumCollisionObjects());
				geom_node->vertex_data = src_geom->vertex_data;
				geom_node->index_data = src_geom->index_data;
				geom_node->radius = radius;
				geom_node->center[0] = 0.0f;
				geom_node->center[1] = 0.0f;
				geom_node->center[2] = 0.0f;

				TransformNode* trans_node = new TransformNode();
				trans_node->name = "DynamicDebris_" + std::to_string(simulation->dynamics_world->getNumCollisionObjects());
				trans_node->matrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
				trans_node->children.push_back(geom_node);

				bool added = false;
				auto try_attach = [&](scenegraph::Node* parent) -> bool {
					for (scenegraph::Node* t_child : parent->children) {
						if (t_child == template_trans) {
							parent->children.push_back(trans_node);
							return true;
						}
					}
					return false;
				};

				for (scenegraph::Node* l1 : scenegraph_root->children) {
					if (try_attach(l1)) { added = true; break; }
					for (scenegraph::Node* l2 : l1->children) {
						if (try_attach(l2)) { added = true; break; }
						for (scenegraph::Node* l3 : l2->children) {
							if (try_attach(l3)) { added = true; break; }
						}
						if (added) break;
					}
					if (added) break;
				}
				if (!added) {
					scenegraph_root->children.push_back(trans_node);
				}

				PhysicsNode* pn = new PhysicsNode();
				pn->collision_shape = shape;
				pn->body = body;
				pn->mass = btScalar(mass);
				pn->transform_node = trans_node;

				int obj_idx = simulation->dynamics_world->getNumCollisionObjects() - 1;
				simulation->collision_node_index.insert(std::make_pair(obj_idx, pn));
				spawned_debris_nodes.push_back(pn);
			}
		}
	}
}

void Application::step() {
	if (!simulation || !simulation->dynamics_world)
		return;

	const float dt = 1.0f / 60.0f;
	update(dt);

	simulation->step();

	btCollisionObjectArray& collision_object_array = simulation->dynamics_world->getCollisionObjectArray();
	for (int j = simulation->dynamics_world->getNumCollisionObjects() - 1; j >= 0; j--) {
		btCollisionObject* obj = collision_object_array[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState()) {
			body->getMotionState()->getWorldTransform(trans);
		} else {
			trans = obj->getWorldTransform();
		}

		// Respawn falling bodies that plummet below arena floor
		if (body && trans.getOrigin().y() < -45.0f) {
			float rx = -8.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 17.0f));
			float ry = 32.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 14.0f));
			float rz = -8.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 24.0f));
			trans.setOrigin(btVector3(rx, ry, rz));
			trans.setRotation(btQuaternion(0, 0, 0, 1));
			body->setWorldTransform(trans);
			if (body->getMotionState()) {
				body->getMotionState()->setWorldTransform(trans);
			}
			float vx = -3.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 6.0f));
			float vy = -8.0f - static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 6.0f));
			float vz = -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 8.0f));
			body->setLinearVelocity(btVector3(vx, vy, vz));
			body->setAngularVelocity(btVector3(2.0f, 4.0f, 1.0f));
			body->activate(true);
		}

		auto itr = simulation->collision_node_index.find(j);
		if (itr != simulation->collision_node_index.end() && itr->second->mass > 0.f) {
			btVector3 origin_bt = trans.getOrigin();
			glm::vec3 pos((float) origin_bt.x(), (float) origin_bt.y(), (float) origin_bt.z());
			glm::mat4 pos_mat = glm::translate(glm::mat4(1.0f), pos);
			btQuaternion rotation_bt = trans.getRotation();
			glm::quat quat(rotation_bt.w(), rotation_bt.x(), rotation_bt.y(), rotation_bt.z());
			glm::mat4 rotation = glm::toMat4(quat);
			itr->second->transform_node->matrix = pos_mat * rotation;
		}
	}

#if !defined(__ANDROID__)
	if (debug_drawer && simulation->dynamics_world && debug_drawer->getDebugMode() != 0) {
		simulation->dynamics_world->debugDrawWorld();
	}
#endif
}

bool Application::pickBody(float screen_x, float screen_y, int window_w, int window_h) {
	if (!picker || !camera) return false;
	return picker->pick(screen_x, screen_y, window_w, window_h, camera);
}

void Application::dragBody(float screen_x, float screen_y, int window_w, int window_h) {
	if (!picker || !camera) return;
	picker->drag(screen_x, screen_y, window_w, window_h, camera);
}

void Application::releaseBody() {
	if (picker) {
		picker->release();
	}
}

bool Application::isPicking() const {
	return picker && picker->isDragging();
}

btRigidBody* Application::shootProjectile(float speed) {
	if (!spawner || !camera) return nullptr;
	return spawner->spawnFromCamera(camera, speed);
}

void Application::toggleDebugDrawer() {
#if !defined(__ANDROID__)
	if (!debug_drawer) return;
	int current = debug_drawer->getDebugMode();
	if (current == 0) {
		debug_drawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe);
	} else if (current == btIDebugDraw::DBG_DrawWireframe) {
		debug_drawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
	} else if (current == (btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb)) {
		debug_drawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawContactPoints);
	} else {
		debug_drawer->setDebugMode(0);
	}
#endif
}

void Application::renderDebug() {
#if !defined(__ANDROID__)
	if (debug_drawer && camera && debug_drawer->getDebugMode() != 0) {
		debug_drawer->render(camera);
		debug_drawer->clear();
	}
#endif
}

int Application::getDebugMode() const {
#if !defined(__ANDROID__)
	return debug_drawer ? debug_drawer->getDebugMode() : 0;
#else
	return 0;
#endif
}

bool Application::loadScenePreset(int preset_index) {
	if (picker) picker->release();
	if (spawner) spawner->reset();
	for (PhysicsNode* p : spawned_debris_nodes) {
		delete p;
	}
	spawned_debris_nodes.clear();
	current_preset_index = preset_index;
	bool result = droidgfxsim::ScenePresets::loadPreset(preset_index, this, nullptr, nullptr);
	if (result && simulation && simulation->dynamics_world) {
#if !defined(__ANDROID__)
		if (debug_drawer) {
			simulation->dynamics_world->setDebugDrawer(debug_drawer);
		}
#endif
		if (picker) {
			picker->setDynamicsWorld(simulation->dynamics_world);
		}
		if (spawner) {
			spawner->setDynamicsWorld(simulation->dynamics_world);
		}
	}
	return result;
}

int Application::getRigidBodyCount() const {
	if (simulation && simulation->dynamics_world) {
		return simulation->dynamics_world->getNumCollisionObjects();
	}
	return 0;
}
