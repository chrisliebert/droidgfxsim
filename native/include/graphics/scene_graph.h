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

typedef struct Vertex
{
	float position[3];
	float normal[3];
	float textureCoordinate[2];
} Vertex;

typedef enum NodeType {
	Geometry,
	Group,
	Material,
	Switch,
	Transform,
} NodeType;

class TransformNode;

typedef struct Node {
	Node();
	//~Node();
	Node* find(std::string& name);
	Node* find(const char* name);
	Node* find(std::string& name, NodeType search_type);
	Node* find(const char* name, NodeType search_type);

	TransformNode* find_transform_node(std::string& search);
	TransformNode* find_transform_node(const char* search);

	std::string name;
	size_t num_children;
	NodeType type;
	std::vector<Node*> children;
} Node;

typedef struct GeometryNode: public Node {
	GeometryNode();
	float center[3];
	float radius;
	std::vector<Vertex> vertex_data;
} GeometryNode;

typedef struct MaterialNode: public Node {
	MaterialNode();
	std::string diffuse_texture;
} MaterialNode;

typedef struct SwitchNode: public Node {
	bool enabled;
} SwitchNode;

typedef struct TransformNode: public Node {
  TransformNode();
	glm::mat4 matrix;
} TransformNode;

//TransformNode* Node::find(std::string& search)
//TransformNode* Node::find(const char* search)

//Node* find_node(std::string& search, Node* node);
//Node* find_node_by_type(std::string& search, Node* node, NodeType type);

//Node* find_node_by_type(std::string& search, Node* node, NodeType type);

template<typename node_t>
void destroy(node_t* node) {
	node_t* root = (node_t*) node;
	if(root == 0) return;
	for(std::vector<Node*>::iterator it = root->children.begin(); it!=root->children.end(); ++it) {
		Node* child = *it;
		assert(child);
		switch(child->type) {
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
	root->children.clear();
	delete root;
	root = 0;
}

} //namespace scenegraph
#endif //_SCENE_GRAPH_H_
