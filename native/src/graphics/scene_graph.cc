#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"

namespace scenegraph {

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

Node* find(std::string& search, Node* node) {
	if (search.compare(node->name) == 0) {
		return node;
	} else {
		Node* n2 = 0;
		for (std::vector<Node*>::iterator it = node->children.begin();
				it != node->children.end(); ++it) {
			Node* c = *it;
			n2 = scenegraph::find(search, c);
			if (n2 != 0)
				return n2;
		}
		return n2;
	}
}

Node* find(std::string& search, Node* node, NodeType type) {
	if (type == node->type && search.compare(node->name) == 0) {
		return node;
	} else {
		Node* n2 = 0;
		for (std::vector<Node*>::iterator it = node->children.begin();
				it != node->children.end(); ++it) {
			n2 = scenegraph::find(search, *it, type);
			if (n2 != 0)
				return n2;
		}
		return n2;
	}
}


Node* Node::find(std::string& search) {
	return scenegraph::find(search, this);
}

Node* Node::find(const char* search) {
	std::string s(search);
	return scenegraph::find(s, this);
}

Node* Node::find(std::string& search, NodeType search_type) {
	return scenegraph::find(search, this, search_type);
}

Node* Node::find(const char* search, NodeType search_type) {
	std::string s(search, search_type);
	return scenegraph::find(s, this, search_type);
}

TransformNode* find_transform_node(std::string& search, Node* root) {
	return (TransformNode*) scenegraph::find(search, root, Transform);
}

TransformNode* find_transform_node(const char* search, Node* root) {
	std::string s(search);
	return scenegraph::find_transform_node(s, root);
}

TransformNode::TransformNode() {
	type = Transform;
	matrix = glm::mat4(1.f);
}

} // namespace scenegraph
