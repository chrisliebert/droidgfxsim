// Copyright (C) 2017 Chris Liebert

#include "common/asset_manager.hpp"
#include "common/log.h"
#include "graphics/wavefront_factory.h"

#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

WavefrontSceneGraphFactory::WavefrontSceneGraphFactory() {
	start_position = 0;
}

WavefrontSceneGraphFactory::~WavefrontSceneGraphFactory() {
	geometry_nodes.clear();
	materials.clear();
	textures.clear();
}


// Used to check file extension
bool hasEnding(std::string const &fullString, std::string const &ending) {
	if (fullString.length() >= ending.length()) {
		return (0
				== fullString.compare(fullString.length() - ending.length(),
						ending.length(), ending));
	} else {
		return false;
	}
}

void WavefrontSceneGraphFactory::addTexture(const char* texture_file_name) {
	std::set<std::string>::const_iterator it = textures.find(texture_file_name);

	if (it == textures.end()) {
		textures.insert(std::string(texture_file_name));
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
	}
}

// Cross-platform directory separator
#ifdef _MSC_VER
#define DIRECTORY_PATH_SEPARATOR "\\"
#else
#define DIRECTORY_PATH_SEPARATOR "/"
#endif

void replaceSubStr(string& source, string& it, string& with) {
	int pos;
	do {
		pos = (int) source.find(it);
		if (pos != -1)
			source.replace(pos, it.length(), with);
	} while (pos != -1);
}

void replaceSubStr(string& source, const char* it, const char* with) {
	std::string its(it);
	std::string withs(with);
	replaceSubStr(source, its, withs);
}

class MaterialStringStreamReader : public tinyobj::MaterialReader {
public:
	MaterialStringStreamReader(const std::string& _mat_sstream)
	: mat_sstream(_mat_sstream) {}
	virtual ~MaterialStringStreamReader() {}
	virtual bool operator()(const std::string& mat_id,
			std::vector<tinyobj::material_t>* materials,
			std::map<std::string, int>* mat_map,
			std::string* err) {
		(void)mat_id;
		std::string warning;
		tinyobj::LoadMtl(mat_map, materials, &mat_sstream, &warning);

		if (!warning.empty()) {
			if (err) {
				(*err) += warning;
			}
		}
		return true;
	}

private:
	std::stringstream mat_sstream;
};

// Find all the .mtl files included in a wavefront .obj source
std::vector<std::string> getMTLFilenames(const std::string& obj_contents) {
	std::vector<std::string> filenames;
	std::stringstream ss(obj_contents);
	std::string line;
	while(std::getline(ss, line, '\n')) {
		size_t pos = line.find("mtllib ");
		if( pos != string::npos) {
			std::string mtl_filename = line.substr(pos+7);
			// Strip spaces and \r
			replaceSubStr(mtl_filename, " ", "");
			replaceSubStr(mtl_filename, "\r", "");
			// Strip comments
			size_t comment_pos = mtl_filename.find("#");
			if(comment_pos != string::npos) {
				mtl_filename = mtl_filename.substr(0, comment_pos);
			}
			filenames.push_back(mtl_filename);
		}
	}
	return filenames;
}

bool WavefrontSceneGraphFactory::addWavefront(const char* file_name, glm::mat4 matrix, AssetManager* asset_manager) {
	size_t initial_num_materials = this->materials.size();
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> material_list;
	std::string err;
	std::stringstream obj_stream;
	std::stringstream mat_stream;
	const std::string obj_contents = asset_manager->loadTextFile(file_name);
	std::vector<std::string> mtl_filenames = getMTLFilenames(obj_contents);
	obj_stream << obj_contents;
	for(std::vector<std::string>::iterator it = mtl_filenames.begin(); it != mtl_filenames.end(); ++it) {
		std::string mtl_filename = *it;
		std::string mtl_contents = asset_manager->loadTextFile(mtl_filename.c_str());
		mat_stream << mtl_contents;
	}
	const std::string mtls_contents = mat_stream.str();
	MaterialStringStreamReader mat_ss_reader(mtls_contents);
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &material_list, &err, &obj_stream, &mat_ss_reader);
	if(!ret) {
		return false;
	}
	std::stringstream file_name_path;
	file_name_path << file_name;
	std::stringstream namess;
	namess << name << "[" << file_name_path.str() << "]";
	this->name = namess.str();
	if (!err.empty()) {
		LOGE("%s\n", err.c_str());
		return false;
	}

	if (!ret) {
		return false;
	}

	// load diffuse textures
	for (size_t mi = 0; mi < material_list.size(); mi++) {
		tinyobj::material_t* mp = &material_list[mi];
		if (mp->diffuse_texname.length() > 0) {
			// Check for paths that are not valid (unix support for paths with \ or \\ instead of /
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

	// Load data
	for (size_t s = 0; s < shapes.size(); s++) {
		GeometryNode* geom_node = new GeometryNode();
		assert(geom_node);
		geom_node->center[0] = 0.f;
		geom_node->center[1] = 0.f;
		geom_node->center[2] = 0.f;

		for (size_t f = 0; f < shapes[s].mesh.indices.size() / 3; f++) {
			tinyobj::index_t idx0 = shapes[s].mesh.indices[3 * f + 0];
			tinyobj::index_t idx1 = shapes[s].mesh.indices[3 * f + 1];
			tinyobj::index_t idx2 = shapes[s].mesh.indices[3 * f + 2];
			int current_material_id = shapes[s].mesh.material_ids[f];
			if ((current_material_id < 0)
					|| (current_material_id >= material_list.size())) {
				// Invalid material ID. Use default material.
				current_material_id = (int) material_list.size(); // Default material is added to the last item in `materialList`.
				LOGE(
						"Invalid material index: %i reverting to default material.",
						current_material_id);
			}

			float diffuse[3];
			for (size_t i = 0; i < 3; i++) {
				diffuse[i] = material_list[current_material_id].diffuse[i];
			}
			float tc[3][2];
			if (attrib.texcoords.size() > 0) {
				if (attrib.texcoords.size() > 2 * idx0.texcoord_index + 1) {
					tc[0][0] = attrib.texcoords[2 * idx0.texcoord_index];
					tc[0][1] = 1.0f
							- attrib.texcoords[2 * idx0.texcoord_index + 1];
				} else {
					tc[0][0] = 0.f;
					tc[0][1] = 0.f;
				}
				if (attrib.texcoords.size() > 2 * idx1.texcoord_index + 1) {
					tc[1][0] = attrib.texcoords[2 * idx1.texcoord_index];
					tc[1][1] = 1.0f
							- attrib.texcoords[2 * idx1.texcoord_index + 1];
				} else {
					tc[1][0] = 0.f;
					tc[1][1] = 0.f;
				}
				if (attrib.texcoords.size() > 2 * idx2.texcoord_index + 1) {
					tc[2][0] = attrib.texcoords[2 * idx2.texcoord_index];
					tc[2][1] = 1.0f
							- attrib.texcoords[2 * idx2.texcoord_index + 1];
				} else {
					tc[2][0] = 0.f;
					tc[2][1] = 0.f;
				}
			} else {
				LOGI("Texture coordinates are not defined");
			}

			float v[3][3];
			for (int k = 0; k < 3; k++) {
				int f0 = idx0.vertex_index;
				int f1 = idx1.vertex_index;
				int f2 = idx2.vertex_index;
				assert(f0 >= 0);
				assert(f1 >= 0);
				assert(f2 >= 0);
				v[0][k] = attrib.vertices[3 * f0 + k];
				v[1][k] = attrib.vertices[3 * f1 + k];
				v[2][k] = attrib.vertices[3 * f2 + k];
			}

			float n[3][3];
			if (attrib.normals.size() > 0) {
				int f0 = idx0.normal_index;
				int f1 = idx1.normal_index;
				int f2 = idx2.normal_index;
				assert(f0 >= 0);
				assert(f1 >= 0);
				assert(f2 >= 0);
				for (int k = 0; k < 3; k++) {
					n[0][k] = attrib.normals[3 * f0 + k];
					n[1][k] = attrib.normals[3 * f1 + k];
					n[2][k] = attrib.normals[3 * f2 + k];
				}
			} else {
				// compute geometric normal
				calcNormal(n[0], v[0], v[1], v[2]);
				n[1][0] = n[0][0];
				n[1][1] = n[0][1];
				n[1][2] = n[0][2];
				n[2][0] = n[0][0];
				n[2][1] = n[0][1];
				n[2][2] = n[0][2];
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

				// local object center mean calculation (stage 1)
				geom_node->center[0] += vert.position[0];
				geom_node->center[1] += vert.position[1];
				geom_node->center[2] += vert.position[2];

				geom_node->vertex_data.push_back(vert);
				//indices.push_back((unsigned) indices.size());
			}
		}

		if (geom_node->vertex_data.size() == 0) {
			// Ignore scene nodes that don't have geometry
			LOGI(
					"Warning, scene node %s does not containing geometry, ommiting.",
					shapes[s].name.c_str());
			continue;
		}

		geom_node->name = shapes[s].name;

		double num_vertices = (double) geom_node->vertex_data.size();
		// 2nd stage of mean calculation

		geom_node->center[0] = geom_node->center[0] / num_vertices;
		geom_node->center[1] = geom_node->center[1] / num_vertices;
		geom_node->center[2] = geom_node->center[2] / num_vertices;

		// Now that the center is calculated, it is subtracted from the individual vertices.
		for (auto it = geom_node->vertex_data.begin();
				it != geom_node->vertex_data.end(); ++it) {
			Vertex* vert = &*it;
			for (int xyz = 0; xyz < 3; xyz++) {
				vert->position[xyz] -= geom_node->center[xyz];
			}
		}

		geom_node->radius = 0.f;
		// Radius calculation

		for (size_t i = 0; i < geom_node->vertex_data.size(); i++) {
			float x = geom_node->vertex_data.at(i).position[0];
			float y = geom_node->vertex_data.at(i).position[1];
			float z = geom_node->vertex_data.at(i).position[2];
			float nx = x - geom_node->center[0];
			float ny = y - geom_node->center[1];
			float nz = z - geom_node->center[2];
			float sum_of_squares = nx * nx + ny * ny + nz * nz;
			if (sum_of_squares > 0.f) {
				float r2 = sqrtf(sum_of_squares);
				if (r2 > geom_node->radius) {
					geom_node->radius = r2;
				}
			}
		}

		if (geom_node->radius <= 0.f) {
			geom_node->radius = 0.1f;
		}

		if (shapes[s].mesh.material_ids.size() > 0
				&& shapes[s].mesh.material_ids.size() > s) {
			size_t node_mat_id = shapes[s].mesh.material_ids[s]
					+ initial_num_materials;
			node_material_association[geom_node] = node_mat_id;
		}

		geometry_nodes.push_back(geom_node);
	}

	if (geometry_nodes.size() == 0) {
		LOGE("Error: No scene nodes defined in %s", file_name);
		return false;
	}

	return true;
}

Node* WavefrontSceneGraphFactory::build() {
	Node* group = new Node();
	assert(group);
	group->name = std::string(name);

	for (std::vector<MaterialNode*>::iterator it = materials.begin();
			it != materials.end(); ++it) {
		MaterialNode* wfm = *it;
		group->children.push_back(wfm);
	}

	for (std::vector<GeometryNode*>::iterator it = this->geometry_nodes.begin();
			it != this->geometry_nodes.end(); ++it) {
		GeometryNode* geom_node = *it;
		size_t mat_id = node_material_association[geom_node];
		std::string mat_name = materials.at(mat_id)->name;
		MaterialNode* mat_node = (MaterialNode*) group->find(mat_name);
		// mat_node should have been added in the previous loop
		assert(mat_node);
		TransformNode* trans_node = new TransformNode();
		assert(trans_node);
		trans_node->matrix = glm::translate(glm::mat4(1.0f),
				glm::vec3(geom_node->center[0], geom_node->center[1],
						geom_node->center[2]));
		trans_node->name = geom_node->name;
		// Rename geometry node
		geom_node->name = trans_node->name + std::string("_Geometry");
		trans_node->children.push_back(geom_node);
		mat_node->children.push_back(trans_node);
	}
	return group;
}
