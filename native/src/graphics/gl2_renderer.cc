// Copyright (C) 2017 Chris Liebert

#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "graphics/gl2_renderer.h"

void GL2SceneGraphRenderer::walk_init_buffers(Node* node) {
	if(node == 0) return;
	if(node->type == NodeType::Geometry) {
		GeometryNode* geometry_node = (GeometryNode*) node;
		assert(geometry_node);
		if(vbos.find(geometry_node) == vbos.end()) {
			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * geometry_node->vertex_data.size(),
					geometry_node->vertex_data.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			vbos.insert(std::make_pair(geometry_node, vbo));

			GLuint ibo;
			glGenBuffers(1, &ibo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * geometry_node->index_data.size(),
					geometry_node->index_data.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

			ibos.insert(std::make_pair(geometry_node, ibo));
		}
	}
	for(std::vector<Node*>::iterator it = node->children.begin(); it!=node->children.end(); ++it) {
		Node* child = *it;
		walk_init_buffers(child);
	}
}

void GL2SceneGraphRenderer::walk_render(Node* node) {
	if(node == 0) return;
	if(node->type == NodeType::Geometry) {
		GeometryNode* geometry_node = (GeometryNode*) node;
		assert(geometry_node);
		if(vbos.find(geometry_node) != vbos.end()) {
			GLuint vbo = vbos[geometry_node];
			GLuint ibo = ibos[geometry_node];
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(0));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),	BUFFER_OFFSET(3 * sizeof(float)));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),	BUFFER_OFFSET(6 * sizeof(float)));
			glDrawElements(GL_TRIANGLES, (GLsizei)(sizeof(GLuint) * geometry_node->index_data.size()), GL_UNSIGNED_INT, BUFFER_OFFSET(0));
			glDisableVertexAttribArray(2);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	} else if(node->type == NodeType::Material) {
		MaterialNode* material_node = (MaterialNode*) node;
		assert(material_node);
		std::map<std::string, GLuint>::iterator texture_id_itr = texture_ids.find(material_node->diffuse_texture);
		if(texture_id_itr == texture_ids.end()) {
			LOGI("Unable to use texture %s", material_node->diffuse_texture.c_str());
		} else {
			glUniform1ui(glGetUniformLocation(shader_program, "diffuseTexture"), 0);
			GLuint texture_id = texture_id_itr->second;
			glBindTexture(GL_TEXTURE_2D, texture_id);
		}
	} else if(node->type == NodeType::Transform) {
		TransformNode* tn = (TransformNode*) node;
		glUniformMatrix4fv(matrix_uniform_location, 1, GL_FALSE, glm::value_ptr(tn->matrix));
	}
	for(std::vector<Node*>::iterator it = node->children.begin(); it!=node->children.end(); ++it) {
		Node* child = *it;
		walk_render(child);
	}
}

GL2SceneGraphRenderer::GL2SceneGraphRenderer(std::map<std::string, Image*>& images) {
	for(std::map<std::string, Image*>::iterator it = images.begin(); it != images.end(); ++it) {
		if(texture_ids.find(it->first) == texture_ids.end()) {
			texture_ids.insert(std::make_pair(it->first, it->second->loadTexture()));
			delete it->second;
			it->second = 0;
		}
	}
	images.clear();

	const char* vertex_shader_src =
		"#version 100																		\n"
		"attribute highp vec3 vPosition;					        			        	\n"
		"attribute highp vec3 vNormal;														\n"
		"attribute highp vec2 vTexCoord;													\n"
		"uniform highp mat4 projection;                										\n"
		"uniform highp mat4 modelview;                  									\n"
		"uniform highp mat4 matrix;               											\n"
		"varying highp vec3 fragPos;														\n"
		"varying highp vec3 normal;															\n"
		"varying highp vec2 texcoord;														\n"
		"varying highp vec3 lightPos;														\n"
		"highp mat4 mat4_inverse(highp mat4 m) {											\n"
		"   highp float																		\n"
		"      a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3],					\n"
		"      a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3],					\n"
		"      a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3],					\n"
		"      a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3],					\n"
		"      b00 = a00 * a11 - a01 * a10,													\n"
		"      b01 = a00 * a12 - a02 * a10,													\n"
		"      b02 = a00 * a13 - a03 * a10,													\n"
		"      b03 = a01 * a12 - a02 * a11,													\n"
		"      b04 = a01 * a13 - a03 * a11,													\n"
		"      b05 = a02 * a13 - a03 * a12,													\n"
		"      b06 = a20 * a31 - a21 * a30,													\n"
		"      b07 = a20 * a32 - a22 * a30,													\n"
		"      b08 = a20 * a33 - a23 * a30,													\n"
		"      b09 = a21 * a32 - a22 * a31,													\n"
		"      b10 = a21 * a33 - a23 * a31,													\n"
		"      b11 = a22 * a33 - a23 * a32,													\n"
		"      det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;	\n"
		"  return mat4(																		\n"
		"      a11 * b11 - a12 * b10 + a13 * b09,											\n"
		"      a02 * b10 - a01 * b11 - a03 * b09,											\n"
		"      a31 * b05 - a32 * b04 + a33 * b03,											\n"
		"      a22 * b04 - a21 * b05 - a23 * b03,											\n"
		"      a12 * b08 - a10 * b11 - a13 * b07,											\n"
		"      a00 * b11 - a02 * b08 + a03 * b07,											\n"
		"      a32 * b02 - a30 * b05 - a33 * b01,											\n"
		"      a20 * b05 - a22 * b02 + a23 * b01,											\n"
		"      a10 * b10 - a11 * b08 + a13 * b06,											\n"
		"      a01 * b08 - a00 * b10 - a03 * b06,											\n"
		"      a30 * b04 - a31 * b02 + a33 * b00,											\n"
		"      a21 * b02 - a20 * b04 - a23 * b00,											\n"
		"      a11 * b07 - a10 * b09 - a12 * b06,											\n"
		"      a00 * b09 - a01 * b07 + a02 * b06,											\n"
		"      a31 * b01 - a30 * b03 - a32 * b00,											\n"
		"      a20 * b03 - a21 * b01 + a22 * b00) / det;									\n"
		"}																					\n"
		"highp mat4 mat4_transpose(mat4 inMatrix) {											\n"
		"     highp vec4 i0 = inMatrix[0];													\n"
		"     highp vec4 i1 = inMatrix[1];													\n"
		"     highp vec4 i2 = inMatrix[2];													\n"
		"     highp vec4 i3 = inMatrix[3];													\n"
		"     highp mat4 outMatrix = mat4(													\n"
		"                 vec4(i0.x, i1.x, i2.x, i3.x),										\n"
		"                 vec4(i0.y, i1.y, i2.y, i3.y),										\n"
		"                 vec4(i0.z, i1.z, i2.z, i3.z),										\n"
		"                 vec4(i0.w, i1.w, i2.w, i3.w)										\n"
		"                 );																\n"
		"    return outMatrix;																\n"
		"}																					\n"
		"void main() {																		\n"
		"	gl_Position = projection * modelview											\n"
		"		* matrix * vec4(vPosition, 1.0);											\n"
		"	fragPos = vec3(modelview * matrix * vec4(vPosition, 1.0));						\n"
		"	normal = mat3(mat4_transpose(mat4_inverse(modelview * matrix))) * vNormal;		\n"
		"	highp vec3 lightPosIn = vec3(0.0, 10.0, 0.0);									\n"
		"	lightPos = vec3(modelview * vec4(lightPosIn, 1.0));								\n"
		"	texcoord = vTexCoord;															\n"
		"}																					\n";

	const char* fragment_shader_src =
		"#version 100                                  										\n"
		"precision mediump float;                                      						\n"
		"varying vec3 fragPos;																\n"
		"varying vec3 normal;																\n"
		"varying vec2 texcoord;																\n"
		"varying vec3 lightPos;																\n"
		"//uniform vec3 lightColor;															\n"
		"//uniform vec3 objectColor;														\n"
		"uniform sampler2D diffuseTexture;													\n"
		"void main()																		\n"
		"{																					\n"
		"	vec3 lightColor = vec3(0.5, 0.5, 0.5);											\n"
		"	vec3 objectColor = texture2D(diffuseTexture, texcoord).rgb;						\n"
		"	float ambientStrength = 0.1;													\n"
		"	vec3 ambient = ambientStrength * lightColor;    								\n"
		"	vec3 norm = normalize(normal);													\n"
		"	vec3 lightDir = normalize(lightPos - fragPos);									\n"
		"	float diff = max(dot(norm, lightDir), 0.0);										\n"
		"	vec3 diffuse = diff * lightColor;												\n"
		"	float specularStrength = 0.5;													\n"
		"	vec3 viewDir = normalize(-fragPos);												\n"
		"	vec3 reflectDir = reflect(-lightDir, norm);  									\n"
		"	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);						\n"
		"	vec3 specular = specularStrength * spec * lightColor; 							\n"
		"	vec3 result = (ambient + diffuse + specular) * objectColor;						\n"
		"	gl_FragColor = vec4(result, 1.0);												\n"
		"}																					\n";

	shader_program = createProgram(vertex_shader_src, fragment_shader_src);
	glUseProgram(shader_program);
	glActiveTexture(GL_TEXTURE0);
	matrix_uniform_location = glGetUniformLocation(shader_program, "matrix");
}

GL2SceneGraphRenderer::~GL2SceneGraphRenderer() {
	for(std::map<GeometryNode*, GLuint>::iterator it = ibos.begin(); it != ibos.end(); it++) {
		GLuint ibo = it->second;
		glDeleteBuffers(1, &ibo);
	}
	for(std::map<GeometryNode*, GLuint>::iterator it = vbos.begin(); it != vbos.end(); it++) {
		GLuint vbo = it->second;
		glDeleteBuffers(1, &vbo);
	}
	for(std::map<std::string, GLuint>::iterator it = texture_ids.begin(); it != texture_ids.end(); ++it) {
		GLuint texture_id = it->second;
		glDeleteTextures(1, &texture_id);
	}
	vbos.clear();
	ibos.clear();
	glDeleteProgram(shader_program);
}

void GL2SceneGraphRenderer::render(Node* node, Camera* camera) {
	glEnable(GL_DEPTH_TEST);
	walk_init_buffers(node);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shader_program);
	GLint projection_location = glGetUniformLocation(shader_program, "projection");//todo save this
	glUniformMatrix4fv(projection_location, 1, GL_FALSE, glm::value_ptr(camera->projection_matrix));
	GLint modelview_location = glGetUniformLocation(shader_program, "modelview");//todo save this
	glUniformMatrix4fv(modelview_location, 1, GL_FALSE, glm::value_ptr(camera->modelview_matrix));
	glUniform3f(glGetUniformLocation(shader_program, "lightPos"), 0.0f, 10.0f, -10.0f);
	glUniform1ui(glGetUniformLocation(shader_program, "diffuseTexture"), 0);
	walk_render(node);
	glUseProgram(0);
}
