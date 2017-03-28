#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"

using namespace scenegraph;

GeometryNode::GeometryNode() {
	type = Geometry;
	radius = 0.f;
}

MaterialNode::MaterialNode() {
	type = Material;
}

Node::Node() {
	type = Group;
	num_children = 0;
}

Node* find_node(std::string& search, Node* node) {
	if (search.compare(node->name) == 0) {
		return node;
	} else {
		Node* n2 = 0;
		for (std::vector<Node*>::iterator it = node->children.begin();
				it != node->children.end(); ++it) {
			n2 = find_node(search, *it);
			if (n2 != 0)
				return n2;
		}
		return n2;
	}
}

Node* find_node_by_type(std::string& search, Node* node, NodeType type) {
	if (type == node->type && search.compare(node->name) == 0) {
		return node;
	} else {
		Node* n2 = 0;
		for (std::vector<Node*>::iterator it = node->children.begin();
				it != node->children.end(); ++it) {
			n2 = find_node_by_type(search, *it, type);
			if (n2 != 0)
				return n2;
		}
		return n2;
	}
}

Node* Node::find(std::string& search) {
	return find_node(search, this);
}

Node* Node::find(const char* search) {
	std::string s(search);
	return find_node(s, this);
}

Node* Node::find(std::string& search, NodeType search_type) {
	return find_node_by_type(search, this, search_type);
}

Node* Node::find(const char* search, NodeType search_type) {
	std::string s(search, search_type);
	return find_node_by_type(s, this, search_type);
}

TransformNode* Node::find_transform_node(std::string& search) {
	return (TransformNode*) find_node_by_type(search, this, Transform);
}

TransformNode* Node::find_transform_node(const char* search) {
	std::string s(search);
	return (TransformNode*) find_node_by_type(s, this, Transform);
}

/*
 TransformNode* Node::find(std::string& search) {
 return find_node(search, this, NodeType::Transform);
 }

 TransformNode* Node::find(const char* search) {
 std::string s(search);
 return (TransformNode*) find_node(s, this, NodeType::Transform);
 }
 */

/*
 ShaderNode::ShaderNode(const char* vertex_src, const char* fragment_src) {
 type = Shader;
 vertex_shader_src = std::string(vertex_src);
 fragment_shader_src = std::string(fragment_shader_src);
 }

 ShaderNode::~ShaderNode() {
 //glDeleteShader(program);
 }*/

TransformNode::TransformNode() {
	type = Transform;
	matrix = glm::mat4(1.f);
}
