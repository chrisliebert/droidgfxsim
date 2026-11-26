// Copyright (C) 2017-2026 Chris Liebert

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

namespace {

bool detachAndDestroyTransformNode(scenegraph::Node* parent, const std::string& target_name) {
	if (!parent) return false;
	for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
		scenegraph::Node* child = *it;
		if (child && child->type == scenegraph::NodeType::Transform && child->name == target_name) {
			parent->children.erase(it);
			scenegraph::destroy(child);
			return true;
		}
		if (detachAndDestroyTransformNode(child, target_name)) {
			return true;
		}
	}
	return false;
}

float unitRand() {
	return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

void sampleHillsideSummitSpawn(const Simulation* sim, float radius,
                               float& rx, float& ry, float& rz, float& vx_small) {
	// Bias spawns across the left ravine, center hill spur, and right ravine within [-15, +15]
	static int lane_counter = 0;
	const int lane = lane_counter % 3;
	lane_counter++;
	if (lane == 0) {
		rx = -15.0f + unitRand() * 10.0f; // [-15.0, -5.0]
	} else if (lane == 1) {
		rx = -4.5f + unitRand() * 9.0f;   // [-4.5, +4.5]
	} else {
		rx = 5.0f + unitRand() * 10.0f;   // [+5.0, +15.0]
	}
	rx = std::clamp(rx, -15.0f, 15.0f);
	rz = -31.0f + unitRand() * 10.0f;     // [-31.0, -21.0]
	const float terrain_h = sim ? sim->getTerrainHeightAt(rx, rz) : 14.0f;
	ry = terrain_h + radius + 0.45f;
	vx_small = -0.6f + unitRand() * 1.2f;
}

void applyDownhillVelocity(btRigidBody* body, float vx_small) {
	if (!body) return;
	body->setLinearVelocity(btVector3(vx_small, -0.3f, 3.2f));
	body->setAngularVelocity(btVector3(2.8f, 0.0f, 0.0f));
	body->activate(true);
}

} // namespace

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
    cascade_interval = 0.35f;
    debris_recycle_index = 0;
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
		} else if (0 == std::string("Terrain").compare(name)) {
			std::string terrain_name("Arena_Terrain");
			std::string heightmap("terrain_heightmap.png");
			std::string texture("terrain_diffuse.png");
			std::string hide_nodes("Arena_Floor");
			int width = 121;
			int length = 121;
			float scale_x = 0.75f;
			float scale_z = 0.75f;
			float min_height = -1.5f;
			float max_height = 24.0f;

			for (rapidxml::xml_attribute<>* attr = node->first_attribute();
					attr; attr = attr->next_attribute()) {
				std::string aname(attr->name() ? attr->name() : "");
				std::string aval(attr->value() ? attr->value() : "");
				if (aname == "name" && !aval.empty()) terrain_name = aval;
				else if (aname == "heightmap" && !aval.empty()) heightmap = aval;
				else if (aname == "texture") texture = aval;
				else if (aname == "width" && !aval.empty()) width = std::atoi(aval.c_str());
				else if (aname == "length" && !aval.empty()) length = std::atoi(aval.c_str());
				else if (aname == "scale_x" && !aval.empty()) scale_x = static_cast<float>(std::atof(aval.c_str()));
				else if (aname == "scale_z" && !aval.empty()) scale_z = static_cast<float>(std::atof(aval.c_str()));
				else if (aname == "min_height" && !aval.empty()) min_height = static_cast<float>(std::atof(aval.c_str()));
				else if (aname == "max_height" && !aval.empty()) max_height = static_cast<float>(std::atof(aval.c_str()));
				else if (aname == "hide_nodes") hide_nodes = aval;
			}

			auto terrain = std::make_unique<droidgfxsim::HeightmapTerrain>();
			terrain->loadFromPNG(heightmap, asset_manager, width, length, scale_x, scale_z, min_height, max_height);

			if (!texture.empty() && images.find(texture) == images.end()) {
				Image* tex_img = new Image(texture.c_str(), asset_manager);
				assert(tex_img);
				images.insert(std::make_pair(texture, tex_img));
			}

			// Detach specified legacy flat/platform meshes when heightmap terrain is active
			if (!hide_nodes.empty()) {
				std::stringstream hs(hide_nodes);
				std::string token;
				while (std::getline(hs, token, ',')) {
					size_t first = token.find_first_not_of(" \t\r\n");
					size_t last = token.find_last_not_of(" \t\r\n");
					if (first != std::string::npos && last != std::string::npos) {
						detachAndDestroyTransformNode(scene_node, token.substr(first, last - first + 1));
					}
				}
			}

			scenegraph::MaterialNode* mat_terrain = new scenegraph::MaterialNode();
			mat_terrain->name = "MaterialTerrain";
			mat_terrain->diffuse_texture = texture;

			scenegraph::TransformNode* trans_terrain = new scenegraph::TransformNode();
			trans_terrain->name = terrain_name;
			trans_terrain->matrix = glm::mat4(1.0f);

			scenegraph::GeometryNode* geom_terrain = terrain->buildGeometryNode(terrain_name + "_Geometry");
			trans_terrain->children.push_back(geom_terrain);
			mat_terrain->children.push_back(trans_terrain);
			scene_node->children.push_back(mat_terrain);

			if (simulation) {
				simulation->addTerrainNode(trans_terrain, std::move(terrain));
			}
		}

		if (node->first_node()) {
			parseXMLNode(node->first_node(), scene_node);
		}
	}
}

scenegraph::Node* Application::loadXML(const char* xml_filename) {
	if (config_file_contents) {
		delete[] config_file_contents;
		config_file_contents = nullptr;
	}
	auto doc = std::make_unique<rapidxml::xml_document<>>();
    config_file_contents = asset_manager->loadTextChars(xml_filename);
    if(config_file_contents == nullptr) {
    	LOGE("Failed to read XML file %s", xml_filename);
    	return nullptr;
    }
	doc->parse<0>(config_file_contents);
	scenegraph::Node* root = parseXML(*doc);
	delete[] config_file_contents;
	config_file_contents = nullptr;
	return root;
}

void Application::init() {
	if (!camera) {
		camera = new Camera();
	}
	assert(camera);
	camera->position = glm::vec3(0.0f, 6.8f, 32.0f);
	camera->horizontal_angle = M_PI;
	camera->vertical_angle = 0.0;
	camera->aim(0.0, 0.04);
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
			float rradius = 0.55f + unitRand() * 0.3f;
			float rx = 0.0f, ry = 0.0f, rz = 0.0f, vx_small = 0.0f;
			sampleHillsideSummitSpawn(simulation, rradius, rx, ry, rz, vx_small);
			float rmass = 2.0f;
			spawnDynamicDebris(rx, ry, rz, rradius, rmass);
		}
	}
}

void Application::spawnDynamicDebris(float x, float y, float z, float radius, float mass) {
	if (!simulation || !simulation->dynamics_world)
		return;

	// Recycle oldest debris if pool limit reached to maintain fixed memory & steady flow
	const size_t max_debris = 36;
	if (spawned_debris_nodes.size() >= max_debris) {
		PhysicsNode* pn = spawned_debris_nodes[debris_recycle_index];
		debris_recycle_index = (debris_recycle_index + 1) % max_debris;
		if (pn && pn->body) {
			btTransform trans;
			trans.setIdentity();
			trans.setOrigin(btVector3(x, y, z));
			pn->body->setWorldTransform(trans);
			if (pn->body->getMotionState()) {
				pn->body->getMotionState()->setWorldTransform(trans);
			}
			applyDownhillVelocity(pn->body, -0.6f + unitRand() * 1.2f);
			if (pn->transform_node) {
				pn->transform_node->matrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
			}
			return;
		}
	}

	// Randomly choose from all 7 object types:
	// 0: Scaled Sphere, 1: Scaled Crate, 2: Crystal, 3: Boulder, 4: Rook, 5: Torus, 6: Glider
	static const char* kTemplateNames[7] = {
		"Ball_00",
		"Crate_00",
		"Prop_Crystal_00",
		"Prop_Boulder_00",
		"Prop_Rook_00",
		"Prop_Torus_00",
		"Prop_Glider_00"
	};

	int kind = rand() % 7;
	TransformNode* template_trans = nullptr;
	if (scenegraph_root) {
		template_trans = scenegraph::find_transform_node(kTemplateNames[kind], scenegraph_root);
		if (!template_trans || template_trans->children.empty()) {
			// Fallback gracefully when running preset scenes without Prop_* templates
			kind = rand() % 2;
			template_trans = scenegraph::find_transform_node(kTemplateNames[kind], scenegraph_root);
			if (!template_trans || template_trans->children.empty()) {
				kind = 1 - kind;
				template_trans = scenegraph::find_transform_node(kTemplateNames[kind], scenegraph_root);
			}
		}
	}

	GeometryNode* src_geom = nullptr;
	if (template_trans && !template_trans->children.empty()) {
		src_geom = dynamic_cast<GeometryNode*>(template_trans->children[0]);
	}

	GeometryNode* geom_node = nullptr;
	btCollisionShape* shape = nullptr;

	if (src_geom && !src_geom->vertex_data.empty()) {
		const int obj_seq = simulation->dynamics_world->getNumCollisionObjects();
		geom_node = new GeometryNode();
		geom_node->name = "DynamicDebrisGeom_" + std::to_string(obj_seq);
		geom_node->vertex_data = src_geom->vertex_data;
		geom_node->index_data = src_geom->index_data;

		if (kind == 0) {
			// Scaled Sphere: scale cloned vertices to match btSphereShape(radius) to < 1e-5
			float src_radius = 0.0f;
			for (const auto& v : geom_node->vertex_data) {
				float r = std::sqrt(v.position[0] * v.position[0]
				                  + v.position[1] * v.position[1]
				                  + v.position[2] * v.position[2]);
				if (r > src_radius) src_radius = r;
			}
			if (src_radius <= 1e-5f) src_radius = 1.0f;
			float scale = radius / src_radius;
			for (auto& v : geom_node->vertex_data) {
				v.position[0] *= scale;
				v.position[1] *= scale;
				v.position[2] *= scale;
			}
			geom_node->computeWelzlBoundingSphere();
			shape = new btSphereShape(radius);
			simulation->collision_shapes.push_back(shape);
		} else if (kind == 1) {
			// Scaled Crate: scale box half-extents to match btBoxShape(radius * 0.8f)
			float target_half = radius * 0.8f;
			float src_half = 0.0f;
			for (const auto& v : geom_node->vertex_data) {
				src_half = std::max({src_half, std::abs(v.position[0]), std::abs(v.position[1]), std::abs(v.position[2])});
			}
			if (src_half <= 1e-5f) src_half = 0.8f;
			float scale = target_half / src_half;
			for (auto& v : geom_node->vertex_data) {
				v.position[0] *= scale;
				v.position[1] *= scale;
				v.position[2] *= scale;
			}
			geom_node->computeWelzlBoundingSphere();
			shape = new btBoxShape(btVector3(target_half, target_half, target_half));
			simulation->collision_shapes.push_back(shape);
		} else if (kind == 5) {
			// Torus wheel: scale visual vertices and 8-sphere compound ring identically
			float src_outer = 0.0f;
			for (const auto& v : geom_node->vertex_data) {
				float r = std::sqrt(v.position[0] * v.position[0]
				                  + v.position[1] * v.position[1]
				                  + v.position[2] * v.position[2]);
				if (r > src_outer) src_outer = r;
			}
			if (src_outer <= 1e-5f) src_outer = 0.96f;
			float scale = radius / src_outer;
			for (auto& v : geom_node->vertex_data) {
				v.position[0] *= scale;
				v.position[1] *= scale;
				v.position[2] *= scale;
			}
			geom_node->computeWelzlBoundingSphere();
			shape = simulation->createTorusCompoundShape(0.72f * scale, 0.24f * scale, 8, 0);
			simulation->collision_shapes.push_back(shape);
		} else {
			// Crystal, Boulder, Rook, Glider: scale visual vertices and construct tight ConvexHull (0.01f margin)
			float src_radius = (src_geom->radius > 1e-5f) ? src_geom->radius : 0.0f;
			if (src_radius <= 1e-5f) {
				for (const auto& v : geom_node->vertex_data) {
					float r = std::sqrt(v.position[0] * v.position[0]
					                  + v.position[1] * v.position[1]
					                  + v.position[2] * v.position[2]);
					if (r > src_radius) src_radius = r;
				}
			}
			if (src_radius <= 1e-5f) src_radius = 1.0f;
			float scale = radius / src_radius;
			for (auto& v : geom_node->vertex_data) {
				v.position[0] *= scale;
				v.position[1] *= scale;
				v.position[2] *= scale;
			}
			geom_node->computeWelzlBoundingSphere();

			btConvexHullShape* hull = new btConvexHullShape();
			for (const auto& v : geom_node->vertex_data) {
				hull->addPoint(btVector3(v.position[0], v.position[1], v.position[2]), false);
			}
			hull->recalcLocalAabb();
			hull->setMargin(0.01f);
			shape = hull;
			simulation->collision_shapes.push_back(shape);
		}
	} else {
		if (kind == 0) {
			shape = new btSphereShape(radius);
		} else {
			shape = new btBoxShape(btVector3(radius * 0.8f, radius * 0.8f, radius * 0.8f));
		}
		simulation->collision_shapes.push_back(shape);
	}

	btTransform start_transform;
	start_transform.setIdentity();
	start_transform.setOrigin(btVector3(x, y, z));

	btDefaultMotionState* motion_state = new btDefaultMotionState(start_transform);
	btVector3 inertia(0.0f, 0.0f, 0.0f);
	shape->calculateLocalInertia(mass, inertia);

	btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, shape, inertia);
	if (kind == 0) {
		rb_info.m_restitution = 0.25f;
		rb_info.m_friction = 0.45f;
		rb_info.m_rollingFriction = 0.08f;
		rb_info.m_linearDamping = 0.12f;
		rb_info.m_angularDamping = 0.12f;
	} else if (kind == 5) {
		rb_info.m_restitution = 0.24f;
		rb_info.m_friction = 0.52f;
		rb_info.m_rollingFriction = 0.04f;
		rb_info.m_linearDamping = 0.10f;
		rb_info.m_angularDamping = 0.10f;
	} else {
		rb_info.m_restitution = 0.18f;
		rb_info.m_friction = 0.55f;
		rb_info.m_linearDamping = 0.14f;
		rb_info.m_angularDamping = 0.14f;
	}

	btRigidBody* body = new btRigidBody(rb_info);
	applyDownhillVelocity(body, -0.6f + unitRand() * 1.2f);

	body->setCcdMotionThreshold(std::min(0.15f, radius * 0.25f));
	body->setCcdSweptSphereRadius(std::min(0.20f, radius * 0.35f));

	simulation->dynamics_world->addRigidBody(body);

	if (scenegraph_root && geom_node && template_trans) {
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
	} else {
		delete geom_node;
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

		// Respawn dynamic falling bodies that roll off the arena bounds
		const btVector3& origin = trans.getOrigin();
		if (body && body->getInvMass() > 0.0f &&
		    !(body->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT) &&
		    (origin.y() < -15.0f || std::abs(origin.x()) > 42.0f ||
		     origin.z() > 38.0f || origin.z() < -43.0f)) {
			float rradius = 1.0f;
			if (body->getCollisionShape()) {
				btVector3 bs_center;
				btScalar bs_radius = 1.0f;
				body->getCollisionShape()->getBoundingSphere(bs_center, bs_radius);
				rradius = std::clamp(static_cast<float>(bs_radius), 0.5f, 2.0f);
			}
			float rx = 0.0f, ry = 0.0f, rz = 0.0f, vx_small = 0.0f;
			sampleHillsideSummitSpawn(simulation, rradius, rx, ry, rz, vx_small);
			trans.setOrigin(btVector3(rx, ry, rz));
			trans.setRotation(btQuaternion(0, 0, 0, 1));
			body->setWorldTransform(trans);
			if (body->getMotionState()) {
				body->getMotionState()->setWorldTransform(trans);
			}
			applyDownhillVelocity(body, vx_small);
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

	int tunnel_count = getFloorTunnelingCount();
	if (tunnel_count > 0) {
		LOGI("[PHYSICS WARNING] Floor tunneling detected: %d dynamic bodies penetrated floor perimeter!", tunnel_count);
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
	const float forward_offset = 2.0f;
	glm::vec3 spawn_pos = camera->position + camera->direction * forward_offset;
	if (simulation && simulation->terrain) {
		const float min_y = simulation->getTerrainHeightAt(spawn_pos.x, spawn_pos.z) + spawner->getRadius() + 0.25f;
		if (spawn_pos.y < min_y) {
			spawn_pos.y = min_y;
		}
	}
	return spawner->spawn(spawn_pos, camera->direction, speed);
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

int Application::getFloorTunnelingCount() const {
	if (!simulation || !simulation->dynamics_world)
		return 0;
	int count = 0;
	const float bound = simulation->terrain ? 43.0f : 28.0f;
	btCollisionObjectArray& objects = simulation->dynamics_world->getCollisionObjectArray();
	for (int i = 0; i < objects.size(); ++i) {
		btCollisionObject* obj = objects[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getInvMass() > 0.0f && !(body->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT)) {
			btTransform trans;
			if (body->getMotionState()) {
				body->getMotionState()->getWorldTransform(trans);
			} else {
				trans = obj->getWorldTransform();
			}
			btVector3 p = trans.getOrigin();
			if (std::abs(p.x()) <= bound && std::abs(p.z()) <= bound) {
				float floor_y = simulation->getTerrainHeightAt(static_cast<float>(p.x()), static_cast<float>(p.z()));
				if (p.y() < floor_y - 0.5f) {
					count++;
				}
			}
		}
	}
	return count;
}
