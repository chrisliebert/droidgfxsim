// Copyright (C) 2017 Chris Liebert

#ifndef _SCENE_GRAPH_H_
#define _SCENE_GRAPH_H_

#include <fstream>
#include <iostream>
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
} Vertex;

typedef enum NodeType {
	Geometry, Group, Material, Switch, Transform,
} NodeType;

class Node {
public:
	Node();
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
	float center[3];
	float radius;
	std::vector<Vertex> vertex_data;
};

class MaterialNode: public Node {
public:
	MaterialNode();
	std::string diffuse_texture;
};

class SwitchNode: public Node {
public:
	bool enabled;
};

class TransformNode: public Node {
public:
	TransformNode();
	glm::mat4 matrix;
};

Node* find(std::string& search, Node* root);
Node* find(std::string& search, Node* root, NodeType type);

TransformNode* find_transform_node(std::string& search, Node* root);
TransformNode* find_transform_node(const char* search, Node* root);

template<typename node_t>
void destroy(node_t* node) {
	node_t* root = (node_t*) node;
	if (root == 0)
		return;
	for (std::vector<Node*>::iterator it = root->children.begin();
			it != root->children.end(); ++it) {
		Node* child = *it;
		if(child) {
			switch (child->type) {
			case Geometry:
				destroy((GeometryNode*) child);
				break;
			case Group:
				destroy(child);
				break;
			case Material:
				destroy((MaterialNode*) child);
				break;
			case Switch:
				destroy((SwitchNode*) child);
				break;
			case Transform:
				destroy((TransformNode*) child);
				break;
			default:
				break;
			}
		}
	}
	root->children.clear();
	delete root;
	root = 0;
}

} //namespace scenegraph
#endif //_SCENE_GRAPH_H_
