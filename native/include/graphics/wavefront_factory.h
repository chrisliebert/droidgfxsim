// Copyright (C) 2017 Chris Liebert

#ifndef _WAVEFRONT_FACTORY_H_
#define _WAVEFRONT_FACTORY_H_

#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>
#include <cassert>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#if defined(__ANDROID__)
	#include <android/asset_manager.h>
	#include <android/asset_manager_jni.h>
#endif

#include "graphics/scene_graph.h"
#include "tiny_obj_loader.h"

#ifdef _MSC_VER
#define strncpy(A,B,C) strncpy_s(A,B,C)
#endif

#define MAX_MATERIAL_NAME_LENGTH 128

using std::cout;
using std::cerr;
using std::endl;
using std::string;

#include "common/asset_manager.hpp"

using namespace scenegraph;

class WavefrontSceneGraphFactory {
public:
	WavefrontSceneGraphFactory();
	~WavefrontSceneGraphFactory();
	void addTexture(const char*);
	bool addWavefront(const char* wavefront_filename, glm::mat4, AssetManager* asset_manager);
	Node* build();
	std::set<std::string> wavefront_files;
	std::set<std::string> textures;
private:
	unsigned start_position;
	std::string name;
	std::vector<GeometryNode*> geometry_nodes;
	std::vector<MaterialNode*> materials;
	std::map<GeometryNode*, size_t> node_material_association;
};

#endif //_WAVEFRONT_FACTORY_H_
