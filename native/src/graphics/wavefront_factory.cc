// Copyright (C) 2017-2026 Chris Liebert

#include "common/asset_manager.hpp"
#include "common/log.h"
#include "common/MathUtil.h"
#include "graphics/wavefront_factory.h"

#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

WavefrontSceneGraphFactory::WavefrontSceneGraphFactory() {
	start_position = 0;
}

WavefrontSceneGraphFactory::~WavefrontSceneGraphFactory() {
	geometry_nodes.clear();
	materials.clear();
	textures.clear();
}

bool hasEnding(std::string const &fullString, std::string const &ending) {
	if (fullString.length() >= ending.length()) {
		return (0 == fullString.compare(fullString.length() - ending.length(),
				ending.length(), ending));
	} else {
		return false;
	}
}

void WavefrontSceneGraphFactory::addTexture(const char* texture_file_name) {
	if (!texture_file_name) return;
	std::string s(texture_file_name);
	if (textures.find(s) == textures.end()) {
		textures.insert(s);
	}
}

void calcNormal(float N[3], float v0[3], float v1[3], float v2[3]) {
	float v10[3];
	v10[0] = v1[0] - v0[0];
	v10[1] = v1[1] - v0[1];
	v10[2] = v1[2] - v0[2];

	float v20[3];
	v20[0] = v2[0] - v0[0];
	v20[1] = v2[1] - v0[1];
	v20[2] = v2[2] - v0[2];

	N[0] = v20[1] * v10[2] - v20[2] * v10[1];
	N[1] = v20[2] * v10[0] - v20[0] * v10[2];
	N[2] = v20[0] * v10[1] - v20[1] * v10[0];

	float len2 = N[0] * N[0] + N[1] * N[1] + N[2] * N[2];
	if (len2 > 0.0f) {
		float len = sqrtf(len2);
		N[0] /= len;
		N[1] /= len;
		N[2] /= len;
	}
}

void replaceSubStr(std::string& source, const std::string& it, const std::string& with) {
	size_t pos = 0;
	while ((pos = source.find(it, pos)) != std::string::npos) {
		source.replace(pos, it.length(), with);
		pos += with.length();
	}
}

void replaceSubStr(std::string& source, const char* it, const char* with) {
	std::string its(it ? it : "");
	std::string withs(with ? with : "");
	replaceSubStr(source, its, withs);
}

class MaterialStringStreamReader : public tinyobj::MaterialReader {
public:
	MaterialStringStreamReader(const std::string& _mat_sstream)
	: mat_sstream(_mat_sstream) {}
	virtual ~MaterialStringStreamReader() override = default;
	virtual bool operator()(const std::string& mat_id,
			std::vector<tinyobj::material_t>* materials,
			std::map<std::string, int>* mat_map,
			std::string* warn,
			std::string* err) override {
		(void)mat_id;
		tinyobj::LoadMtl(mat_map, materials, &mat_sstream, warn, err);
		return true;
	}

private:
	std::stringstream mat_sstream;
};

std::vector<std::string> getMTLFilenames(const std::string& obj_contents) {
	std::vector<std::string> filenames;
	std::stringstream ss(obj_contents);
	std::string line;
	while(std::getline(ss, line, '\n')) {
		size_t pos = line.find("mtllib ");
		if( pos != std::string::npos) {
			std::string mtl_filename = line.substr(pos + 7);
			replaceSubStr(mtl_filename, " ", "");
			replaceSubStr(mtl_filename, "\r", "");
			size_t comment_pos = mtl_filename.find("#");
			if(comment_pos != std::string::npos) {
				mtl_filename = mtl_filename.substr(0, comment_pos);
			}
			if(!mtl_filename.empty()) {
				filenames.push_back(mtl_filename);
			}
		}
	}
	return filenames;
}

bool WavefrontSceneGraphFactory::addWavefront(const char* file_name, glm::mat4 matrix, AssetManager* asset_manager) {
	(void)matrix;
	if (!file_name || !asset_manager) return false;
	std::string filename_str(file_name);
	if(wavefront_files.find(filename_str) == wavefront_files.end()) {
		wavefront_files.insert(filename_str);
	} else {
		LOGI("File %s was already loaded", file_name);
		return true;
	}

	size_t initial_num_materials = this->materials.size();
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> material_list;
	std::string warn, err;
	std::stringstream obj_stream;
	std::stringstream mat_stream;

	const std::string obj_contents = asset_manager->loadTextFile(file_name);
	if (obj_contents.empty()) {
		LOGE("Failed to load OBJ file: %s", file_name);
		return false;
	}
	std::vector<std::string> mtl_filenames = getMTLFilenames(obj_contents);
	obj_stream << obj_contents;
	for(const auto& mtl_filename : mtl_filenames) {
		std::string mtl_contents = asset_manager->loadTextFile(mtl_filename.c_str());
		mat_stream << mtl_contents;
	}
	const std::string mtls_contents = mat_stream.str();
	MaterialStringStreamReader mat_ss_reader(mtls_contents);
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &material_list, &warn, &err, &obj_stream, &mat_ss_reader);

	if (!warn.empty()) {
		LOGI("tinyobj warning: %s", warn.c_str());
	}
	if (!ret) {
		LOGE("tinyobj error: %s", err.c_str());
		return false;
	}

	std::stringstream namess;
	namess << name << "[" << file_name << "]";
	this->name = namess.str();

	// Ensure default material exists if none provided
	if (material_list.empty()) {
		tinyobj::material_t default_mat;
		default_mat.name = "default_material";
		default_mat.diffuse[0] = 0.8f;
		default_mat.diffuse[1] = 0.8f;
		default_mat.diffuse[2] = 0.8f;
		default_mat.diffuse_texname = "";
		material_list.push_back(default_mat);
	}

	// Load diffuse textures and create material nodes
	for (size_t mi = 0; mi < material_list.size(); mi++) {
		tinyobj::material_t* mp = &material_list[mi];
		if (mp->diffuse_texname.length() > 0) {
#ifndef _MSC_VER
			replaceSubStr(mp->diffuse_texname, "\\\\", "/");
			replaceSubStr(mp->diffuse_texname, "\\", "/");
#endif
			addTexture(mp->diffuse_texname.c_str());
		}

		MaterialNode* mat_node = new MaterialNode();
		assert(mat_node);
		mat_node->name = mp->name;
		mat_node->diffuse_texture = mp->diffuse_texname;
		materials.push_back(mat_node);
	}

	// Load geometry data
	for (size_t s = 0; s < shapes.size(); s++) {
		GeometryNode* geom_node = new GeometryNode();
		assert(geom_node);
		std::vector<Vertex> vertices;

		for (size_t f = 0; f < shapes[s].mesh.indices.size() / 3; f++) {
			tinyobj::index_t idx0 = shapes[s].mesh.indices[3 * f + 0];
			tinyobj::index_t idx1 = shapes[s].mesh.indices[3 * f + 1];
			tinyobj::index_t idx2 = shapes[s].mesh.indices[3 * f + 2];

			float tc[3][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
			if (idx0.texcoord_index >= 0 && attrib.texcoords.size() > (size_t)(2 * idx0.texcoord_index + 1)) {
				tc[0][0] = attrib.texcoords[2 * idx0.texcoord_index];
				tc[0][1] = 1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1];
			}
			if (idx1.texcoord_index >= 0 && attrib.texcoords.size() > (size_t)(2 * idx1.texcoord_index + 1)) {
				tc[1][0] = attrib.texcoords[2 * idx1.texcoord_index];
				tc[1][1] = 1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1];
			}
			if (idx2.texcoord_index >= 0 && attrib.texcoords.size() > (size_t)(2 * idx2.texcoord_index + 1)) {
				tc[2][0] = attrib.texcoords[2 * idx2.texcoord_index];
				tc[2][1] = 1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1];
			}

			float v[3][3];
			int f0 = idx0.vertex_index;
			int f1 = idx1.vertex_index;
			int f2 = idx2.vertex_index;
			for (int k = 0; k < 3; k++) {
				v[0][k] = attrib.vertices[3 * f0 + k];
				v[1][k] = attrib.vertices[3 * f1 + k];
				v[2][k] = attrib.vertices[3 * f2 + k];
			}

			float n[3][3];
			if (!attrib.normals.empty() && idx0.normal_index >= 0 && idx1.normal_index >= 0 && idx2.normal_index >= 0) {
				for (int k = 0; k < 3; k++) {
					n[0][k] = attrib.normals[3 * idx0.normal_index + k];
					n[1][k] = attrib.normals[3 * idx1.normal_index + k];
					n[2][k] = attrib.normals[3 * idx2.normal_index + k];
				}
			} else {
				calcNormal(n[0], v[0], v[1], v[2]);
				n[1][0] = n[0][0]; n[1][1] = n[0][1]; n[1][2] = n[0][2];
				n[2][0] = n[0][0]; n[2][1] = n[0][1]; n[2][2] = n[0][2];
			}

			for (int k = 0; k < 3; k++) {
				Vertex vert;
				vert.position[0] = v[k][0];
				vert.position[1] = v[k][1];
				vert.position[2] = v[k][2];
				vert.normal[0] = n[k][0];
				vert.normal[1] = n[k][1];
				vert.normal[2] = n[k][2];
				vert.texcoord[0] = tc[k][0];
				vert.texcoord[1] = tc[k][1];
				vertices.push_back(vert);
			}
		}

		if (vertices.empty()) {
			delete geom_node;
			continue;
		}

		geom_node->name = shapes[s].name;

		// Deduplicate vertices and build indexed geometry
		std::unordered_map<Vertex, GLuint> unique_vertices;
		for(const auto& vert : vertices) {
			auto it = unique_vertices.find(vert);
			if (it == unique_vertices.end()) {
				GLuint vindex = (GLuint) geom_node->vertex_data.size();
				unique_vertices.insert(std::make_pair(vert, vindex));
				geom_node->vertex_data.push_back(vert);
				geom_node->index_data.push_back(vindex);
			} else {
				geom_node->index_data.push_back(it->second);
			}
		}

		// Compute exact Welzl minimal bounding sphere
		geom_node->computeWelzlBoundingSphere();

		// Center the geometry around (0,0,0) and store the center offset
		for (auto& vert : geom_node->vertex_data) {
			for (int xyz = 0; xyz < 3; xyz++) {
				vert.position[xyz] -= geom_node->center[xyz];
			}
		}

		// Associate material: use shape material if valid, else default
		size_t node_mat_id = initial_num_materials;
		if (!shapes[s].mesh.material_ids.empty() && shapes[s].mesh.material_ids[0] >= 0) {
			size_t m_id = (size_t) shapes[s].mesh.material_ids[0];
			if (m_id < material_list.size()) {
				node_mat_id = initial_num_materials + m_id;
			}
		}
		node_material_association[geom_node] = node_mat_id;
		geometry_nodes.push_back(geom_node);
	}

	if (geometry_nodes.empty()) {
		LOGE("Error: No scene nodes defined in %s", file_name);
		return false;
	}

	return true;
}

Node* WavefrontSceneGraphFactory::build() {
	Node* group = new Node();
	assert(group);
	group->name = std::string(name);

	if (materials.empty()) {
		MaterialNode* fallback = new MaterialNode();
		fallback->name = "default_material";
		materials.push_back(fallback);
	}

	for (MaterialNode* wfm : materials) {
		group->children.push_back(wfm);
	}

	for (GeometryNode* geom_node : this->geometry_nodes) {
		size_t mat_id = 0;
		auto it = node_material_association.find(geom_node);
		if (it != node_material_association.end() && it->second < materials.size()) {
			mat_id = it->second;
		}
		MaterialNode* mat_node = materials[mat_id];

		TransformNode* trans_node = new TransformNode();
		assert(trans_node);
		trans_node->matrix = glm::translate(glm::mat4(1.0f),
				glm::vec3(geom_node->center[0], geom_node->center[1], geom_node->center[2]));
		trans_node->name = geom_node->name;
		geom_node->name = trans_node->name + std::string("_Geometry");
		trans_node->children.push_back(geom_node);
		mat_node->children.push_back(trans_node);
	}
	return group;
}
