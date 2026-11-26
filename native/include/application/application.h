// Copyright (C) 2017-2026 Chris Liebert

#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <cassert>
#include <map>
#include <memory>
#include <string>

#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

#include "graphics/camera.h"
#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "graphics/wavefront_factory.h"
#include "graphics/renderer_interface.h"
#include "physics/simulation.h"
#include "physics/terrain.h"

namespace droidgfxsim {
class RaycastPicker;
class ProjectileSpawner;
}
class BulletDebugDrawer;

class Application {
public:
	scenegraph::Node* scenegraph_root;
	Simulation* simulation;
	Camera* camera;
	std::map<std::string, Image*> images;
	char* config_file_contents;

	droidgfxsim::RaycastPicker* picker;
	droidgfxsim::ProjectileSpawner* spawner;
	BulletDebugDrawer* debug_drawer;
	int current_preset_index;

	AssetManager* asset_manager;
#if defined(__ANDROID__)
	Application(AAssetManager* asset_manager);
#else
	Application();
#endif
	~Application();
	scenegraph::Node* parseXML(rapidxml::xml_document<>& doc);
	void parseXMLNode(rapidxml::xml_node<>* xml_node,
			scenegraph::Node* scene_node);
	scenegraph::Node* loadXML(const char* xml_filename);
	scenegraph::Node* loadResources();

	void init();
	void step();

	void render(IRenderer* renderer) {
		if (renderer && scenegraph_root && camera) {
			renderer->render(scenegraph_root, camera);
		}
	}

	void resize(int width, int height);
	void setCameraFov(float fov_degrees);

	// Interactive physics & debug controls
	bool cascade_mode_active;
	float cascade_timer;
	float cascade_interval;
	float cascade_spawn_rate;
	int max_debris_pool;
	std::vector<PhysicsNode*> spawned_debris_nodes;
	size_t debris_recycle_index;

	void spawnDynamicDebris(float x, float y, float z, float radius = 0.8f, float mass = 2.0f);
	void spawnSummitWave(int count = 20);
	void update(float dt = 1.0f / 60.0f);

	bool pickBody(float screen_x, float screen_y, int window_w, int window_h);
	void dragBody(float screen_x, float screen_y, int window_w, int window_h);
	void releaseBody();
	bool isPicking() const;

	btRigidBody* shootProjectile(float speed = 35.0f);

	void toggleDebugDrawer();
	void renderDebug();
	int getDebugMode() const;

	bool loadScenePreset(int preset_index);
	int getRigidBodyCount() const;
	int getFloorTunnelingCount() const;
};

#endif //_APPLICATION_H_
