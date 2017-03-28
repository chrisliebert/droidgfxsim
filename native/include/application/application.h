#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <cassert>

#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

#include "graphics/camera.h"
#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "graphics/wavefront_factory.h"

#include "physics/simulation.h"

class Application {
private:
	float grey;
public:
	scenegraph::Node* scenegraph_root;
	Simulation* simulation;
	Camera* camera;
	std::map<std::string, Image*> images;
	char* config_file_contents;

	AssetManager* asset_manager;
#if defined(__ANDROID__)
	Application(AAssetManager* asset_manager);
#else
	Application();
#endif
	~Application();
	Node* parseXML(rapidxml::xml_document<>& doc);
	void parseXMLNode(rapidxml::xml_node<>* xml_node,
			scenegraph::Node* scene_node);
	Node* loadXML(const char* xml_filename);
	Node* loadResources();

	void init();
	void step();

	template<typename SceneGraphRenderer_T>
	void render(SceneGraphRenderer_T* renderer) {
		renderer->render(scenegraph_root, camera);
	}

	void resize(int width, int height);
};

#endif //_APPLICATION_H_
