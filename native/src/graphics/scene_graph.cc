// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "common/MathUtil.h"

namespace scenegraph {

bool Vertex::operator==(const Vertex& other) const {
	for(int i=0; i<3; i++) {
		if(position[i] != other.position[i]) {
			return false;
		}
		if(normal[i] != other.normal[i]) {
			return false;
		}
		if(i < 2 && texcoord[i] != other.texcoord[i]) {
			return false;
		}
	}
	return true;
}

Node::Node() {
	type = Group;
	num_children = 0;
}

Node::~Node() {
	// Virtual destructor allows safe polymorphic deletion
}

GeometryNode::GeometryNode() {
	type = Geometry;
	center[0] = 0.0f;
	center[1] = 0.0f;
	center[2] = 0.0f;
	radius = 0.0f;
}

void GeometryNode::computeWelzlBoundingSphere() {
	if (vertex_data.empty()) {
		center[0] = center[1] = center[2] = 0.0f;
		radius = 0.0f;
		return;
	}
	std::vector<glm::vec3> points;
	points.reserve(vertex_data.size());
	for (const auto& v : vertex_data) {
		points.emplace_back(v.position[0], v.position[1], v.position[2]);
	}
	Math::Sphere sphere = Math::calculateBoundingSphere(points.data(), points.size());
	center[0] = sphere.center.x;
	center[1] = sphere.center.y;
	center[2] = sphere.center.z;
	radius = sphere.radius;
}

MaterialNode::MaterialNode() {
	type = Material;
}

SwitchNode::SwitchNode() {
	type = Switch;
	enabled = true;
}

TransformNode::TransformNode() {
	type = Transform;
	matrix = glm::mat4(1.f);
}

Node* find(std::string& search, Node* node) {
	if (node == nullptr) return nullptr;
	if (search.compare(node->name) == 0) {
		return node;
	}
	for (Node* c : node->children) {
		Node* n2 = scenegraph::find(search, c);
		if (n2 != nullptr) return n2;
	}
	return nullptr;
}

Node* find(std::string& search, Node* node, NodeType type) {
	if (node == nullptr) return nullptr;
	if (type == node->type && search.compare(node->name) == 0) {
		return node;
	}
	for (Node* c : node->children) {
		Node* n2 = scenegraph::find(search, c, type);
		if (n2 != nullptr) return n2;
	}
	return nullptr;
}

Node* Node::find(std::string& search) {
	return scenegraph::find(search, this);
}

Node* Node::find(const char* search) {
	std::string s(search ? search : "");
	return scenegraph::find(s, this);
}

Node* Node::find(std::string& search, NodeType search_type) {
	return scenegraph::find(search, this, search_type);
}

Node* Node::find(const char* search, NodeType search_type) {
	std::string s(search ? search : "");
	return scenegraph::find(s, this, search_type);
}

TransformNode* find_transform_node(std::string& search, Node* root) {
	return (TransformNode*) scenegraph::find(search, root, Transform);
}

TransformNode* find_transform_node(const char* search, Node* root) {
	std::string s(search ? search : "");
	return scenegraph::find_transform_node(s, root);
}

} // namespace scenegraph
