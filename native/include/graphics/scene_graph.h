// Copyright (C) 2017-2026 Chris Liebert

#ifndef _SCENE_GRAPH_H_
#define _SCENE_GRAPH_H_

#include <fstream>
#include <iostream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics/gl_code.h"

namespace scenegraph {

typedef struct Vertex {
	float position[3];
	float normal[3];
	float texcoord[2];

	bool operator==(const Vertex& other) const;
} Vertex;

typedef enum NodeType {
	Geometry, Group, Material, Switch, Transform,
} NodeType;

class Node {
public:
	Node();
	virtual ~Node();

	Node* find(std::string& name);
	Node* find(const char* name);
	Node* find(std::string& name, NodeType search_type);
	Node* find(const char* name, NodeType search_type);

	std::string name;
	size_t num_children;
	NodeType type;
	std::vector<Node*> children;
};

class GeometryNode: public Node {
public:
	GeometryNode();
	virtual ~GeometryNode() override = default;

	float center[3];
	float radius;
	std::vector<Vertex> vertex_data;
	std::vector<GLuint> index_data;

	void computeWelzlBoundingSphere();
};

class MaterialNode: public Node {
public:
	MaterialNode();
	virtual ~MaterialNode() override = default;

	std::string diffuse_texture;
};

class SwitchNode: public Node {
public:
	SwitchNode();
	virtual ~SwitchNode() override = default;

	bool enabled;
};

class TransformNode: public Node {
public:
	TransformNode();
	virtual ~TransformNode() override = default;

	glm::mat4 matrix;
};

Node* find(std::string& search, Node* root);
Node* find(std::string& search, Node* root, NodeType type);

TransformNode* find_transform_node(std::string& search, Node* root);
TransformNode* find_transform_node(const char* search, Node* root);

inline void destroy(Node* root) {
	if (root == nullptr) return;
	for (Node* child : root->children) {
		destroy(child);
	}
	root->children.clear();
	delete root;
}

} // namespace scenegraph

// Hash function for Vertex
namespace std {
    template<> struct hash<scenegraph::Vertex> {
        size_t operator()(scenegraph::Vertex const& vertex) const {
            size_t h = 0;
            for(int i=0; i<3; i++) {
            	if(i == 0) {
            		h = hash<float>()(vertex.position[0]);
            		h = h ^ (hash<float>()(vertex.normal[0]) << 1);
            	} else {
            		h = h ^ (hash<float>()(vertex.position[i]) << 1);
            		h = h ^ (hash<float>()(vertex.normal[i]) << 1);
            	}

            	if(i != 2) {
            		h = h ^ (hash<float>()(vertex.texcoord[i]) << 1);
            	}
            }
            return h;
        }
    };
}

#endif //_SCENE_GRAPH_H_
