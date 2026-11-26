// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "graphics/gl3_renderer.h"

void GL3SceneGraphRenderer::walk_init_buffers(Node* node) {
	if (node == nullptr) { return; }
	if (node->type == NodeType::Geometry) {
		GeometryNode* geometry_node = (GeometryNode*) node;
		assert(geometry_node);
		if (vaos.find(geometry_node) == vaos.end()) {
			GLuint vao = 0, vbo = 0, ibo = 0;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER,
					sizeof(Vertex) * geometry_node->vertex_data.size(),
					geometry_node->vertex_data.data(), GL_STATIC_DRAW);

			glGenBuffers(1, &ibo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER,
					sizeof(GLuint) * geometry_node->index_data.size(),
					geometry_node->index_data.data(), GL_STATIC_DRAW);

			// Attribute 0: position
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
					BUFFER_OFFSET(offsetof(Vertex, position)));

			// Attribute 1: normal
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
					BUFFER_OFFSET(offsetof(Vertex, normal)));

			// Attribute 2: texcoord
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
					BUFFER_OFFSET(offsetof(Vertex, texcoord)));

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

			vaos.insert(std::make_pair(geometry_node, vao));
			vbos.insert(std::make_pair(geometry_node, vbo));
			ibos.insert(std::make_pair(geometry_node, ibo));
		}
	}
	for (std::vector<Node*>::iterator it = node->children.begin();
			it != node->children.end(); ++it) {
		Node* child = *it;
		walk_init_buffers(child);
	}
}

void GL3SceneGraphRenderer::walk_render(Node* node) {
	if (node == nullptr) return;
	if (node->type == NodeType::Geometry) {
		GeometryNode* geometry_node = (GeometryNode*) node;
		assert(geometry_node);
		std::map<GeometryNode*, GLuint>::iterator vao_node = vaos.find(geometry_node);
		if (vao_node != vaos.end()) {
			GLuint vao = vao_node->second;
			glBindVertexArray(vao);
			glDrawElements(GL_TRIANGLES, (GLsizei)geometry_node->index_data.size(), GL_UNSIGNED_INT, BUFFER_OFFSET(0));
			glBindVertexArray(0);
		}
	} else if (node->type == NodeType::Material) {
		MaterialNode* material_node = (MaterialNode*) node;
		assert(material_node);
		GLuint texture_id = default_texture_id;
		if (!material_node->diffuse_texture.empty()) {
			std::map<std::string, GLuint>::iterator texture_id_itr =
					texture_ids.find(material_node->diffuse_texture);
			if (texture_id_itr != texture_ids.end()) {
				texture_id = texture_id_itr->second;
			}
		}
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		GLint tex_loc = glGetUniformLocation(shader_program, "diffuseTexture");
		if (tex_loc >= 0) {
			glUniform1i(tex_loc, 0);
		}
	} else if (node->type == NodeType::Transform) {
		TransformNode* tn = (TransformNode*) node;
		assert(tn);
		GLint matrix_loc = static_cast<GLint>(matrix_uniform_location);
		if (matrix_loc >= 0) {
			glUniformMatrix4fv(matrix_loc, 1, GL_FALSE,
					glm::value_ptr(tn->matrix));
		}
	}
	for (std::vector<Node*>::iterator it = node->children.begin();
			it != node->children.end(); ++it) {
		Node* child = *it;
		walk_render(child);
	}
}

GL3SceneGraphRenderer::GL3SceneGraphRenderer(std::map<std::string, Image*>& images) {
	default_texture_id = 0;
	const unsigned char white_pixel[4] = { 255, 255, 255, 255 };
	glGenTextures(1, &default_texture_id);
	glBindTexture(GL_TEXTURE_2D, default_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
	glBindTexture(GL_TEXTURE_2D, 0);

	for(std::map<std::string, Image*>::iterator it = images.begin(); it != images.end(); ++it) {
		if(texture_ids.find(it->first) == texture_ids.end()) {
			texture_ids.insert(std::make_pair(it->first, it->second->loadTexture()));
			delete it->second;
			it->second = 0;
		}
	}
	images.clear();

	const char* vertex_shader_src =
			"#version 300 es                            												\n"
					"layout(location = 0) in vec3 vPosition;					        	    		\n"
					"layout(location = 1) in vec3 vNormal;												\n"
					"layout(location = 2) in vec2 vTexCoord;											\n"
					"layout (std140) uniform TransformBlock {											\n"
					"	mat4 projection;               													\n"
					"	mat4 modelview;               													\n"
					"};																					\n"
					"uniform mat4 matrix;																\n"
					"out highp vec3 fragPos;																\n"
					"out highp vec3 normal;																\n"
					"out highp vec3 worldPos;															\n"
					"out highp vec3 worldNormal;															\n"
					"out highp vec2 texcoord;															\n"
					"out highp vec3 lightPos;															\n"
					"void main() {																		\n"
					"	highp vec4 wp = matrix * vec4(vPosition, 1.0);									\n"
					"	gl_Position = projection * modelview * wp;										\n"
					"	fragPos = vec3(modelview * wp);													\n"
					"	worldPos = wp.xyz;																\n"
					"	normal = mat3(transpose(inverse(modelview * matrix))) * vNormal;				\n"
					"	worldNormal = mat3(matrix) * vNormal;											\n"
					"	vec3 lightPosIn = vec3(22.0, 44.0, 28.0);										\n"
					"	lightPos = vec3(modelview * vec4(lightPosIn, 1.0));								\n"
					"	texcoord = vTexCoord;															\n"
					"}																					\n";

	const char* fragment_shader_src =
			"#version 300 es																			\n"
					"precision mediump float;                           			      	 			\n"
					"in highp vec3 fragPos;																\n"
					"in highp vec3 normal;																\n"
					"in highp vec3 worldPos;															\n"
					"in highp vec3 worldNormal;															\n"
					"in highp vec2 texcoord;															\n"
					"in highp vec3 lightPos;															\n"
					"out vec4 color;																	\n"
					"uniform sampler2D diffuseTexture;													\n"
					"highp float hash21(highp vec2 p) {													\n"
					"	p = fract(p * vec2(123.34, 456.21));											\n"
					"	p += dot(p, p + 45.32);															\n"
					"	return fract(p.x * p.y);														\n"
					"}																					\n"
					"highp float valueNoise(highp vec2 p) {												\n"
					"	highp vec2 i = floor(p);														\n"
					"	highp vec2 f = fract(p);														\n"
					"	highp vec2 u = f * f * (3.0 - 2.0 * f);											\n"
					"	highp float a = hash21(i);														\n"
					"	highp float b = hash21(i + vec2(1.0, 0.0));										\n"
					"	highp float c = hash21(i + vec2(0.0, 1.0));										\n"
					"	highp float d = hash21(i + vec2(1.0, 1.0));										\n"
					"	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);								\n"
					"}																					\n"
					"void main() {																		\n"
					"	// Hemisphere sky/earth ambient													\n"
					"	vec3 wNorm = (dot(worldNormal, worldNormal) > 0.0001)							\n"
					"		? normalize(worldNormal) : vec3(0.0, 1.0, 0.0);								\n"
					"	vec3 hemiAmbient = mix(vec3(0.32, 0.28, 0.24), vec3(0.58, 0.72, 0.88),			\n"
					"		clamp(wNorm.y * 0.5 + 0.5, 0.0, 1.0)) * 0.42;								\n"
					"																					\n"
					"	// Sun key light (diffuse + subtle Blinn-Phong specular)						\n"
					"	vec3 lightColor = vec3(1.0, 0.97, 0.91);										\n"
					"	vec3 norm = (dot(normal, normal) > 0.0001)										\n"
					"		? normalize(normal) : vec3(0.0, 0.0, 1.0);									\n"
					"	vec3 lightDir = normalize(lightPos - fragPos);									\n"
					"	float diff = max(dot(norm, lightDir), 0.0);										\n"
					"	vec3 diffuse = diff * lightColor;												\n"
					"																					\n"
					"	vec3 viewDir = normalize(-fragPos);												\n"
					"	vec3 halfwayDir = normalize(lightDir + viewDir);								\n"
					"	float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);						\n"
					"	vec3 specular = 0.18 * spec * lightColor;										\n"
					"																					\n"
					"	vec3 result = hemiAmbient + diffuse + specular;									\n"
					"	vec4 texColor = texture(diffuseTexture, texcoord);								\n"
					"																					\n"
					"	// Multi-scale world-space micro-grain modulation faded over distance			\n"
					"	float dist = length(fragPos);													\n"
					"	highp float nCoarse = valueNoise(worldPos.xz * 2.0);							\n"
					"	highp float nFine = valueNoise(worldPos.xz * 8.333 + vec2(17.3, 9.1));			\n"
					"	float grain = 0.60 * nCoarse + 0.40 * nFine;									\n"
					"	float detailFade = 1.0 - smoothstep(10.0, 42.0, dist);							\n"
					"	float microMod = mix(1.0, 0.90 + 0.20 * grain, detailFade);						\n"
					"																					\n"
					"	vec3 litColor = (texColor.rgb * microMod) * result;								\n"
					"	float fogFactor = smoothstep(48.0, 120.0, dist);								\n"
					"	vec3 fogColor = vec3(0.55, 0.68, 0.82);											\n"
					"	color = vec4(mix(litColor, fogColor, fogFactor), texColor.a);					\n"
					"}																					\n";

	shader_program = createProgram(vertex_shader_src, fragment_shader_src);

	// Uniform buffer object
	binding_point_index = 0;
	transform_block_id = GL_INVALID_INDEX;
	uniform_transform_buffer_block_size = 0;
	uniform_transform_buffer_id = 0;
	matrix_uniform_location = static_cast<GLuint>(-1);
	if (shader_program != 0) {
		transform_block_id = glGetUniformBlockIndex(shader_program,
				"TransformBlock");
		if (transform_block_id != GL_INVALID_INDEX) {
			glUniformBlockBinding(shader_program, transform_block_id,
					binding_point_index);
			glGetActiveUniformBlockiv(shader_program, transform_block_id,
					GL_UNIFORM_BLOCK_DATA_SIZE, &uniform_transform_buffer_block_size);
			if (uniform_transform_buffer_block_size > 0) {
				glGenBuffers(1, &uniform_transform_buffer_id);
				glBindBuffer(GL_UNIFORM_BUFFER, uniform_transform_buffer_id);
				glBufferData(GL_UNIFORM_BUFFER, uniform_transform_buffer_block_size, NULL,
						GL_DYNAMIC_DRAW);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
				glBindBufferBase(GL_UNIFORM_BUFFER, binding_point_index,
						uniform_transform_buffer_id);
			}
		}

		matrix_uniform_location = static_cast<GLuint>(glGetUniformLocation(shader_program, "matrix"));
	}
}

GL3SceneGraphRenderer::~GL3SceneGraphRenderer() {
	if(shader_program) {
		glDeleteProgram(shader_program);
		shader_program = 0;
	}
	if(uniform_transform_buffer_id) {
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding_point_index, 0);
		glDeleteBuffers(1, &uniform_transform_buffer_id);
		uniform_transform_buffer_id = 0;
	}
	for(auto pair : vaos) {
		glDeleteVertexArrays(1, &pair.second);
	}
	vaos.clear();
	for(auto pair : vbos) {
		glDeleteBuffers(1, &pair.second);
	}
	vbos.clear();
	for(auto pair : ibos) {
		glDeleteBuffers(1, &pair.second);
	}
	ibos.clear();
	for(auto pair : texture_ids) {
		glDeleteTextures(1, &pair.second);
	}
	texture_ids.clear();
	if(default_texture_id) {
		glDeleteTextures(1, &default_texture_id);
		default_texture_id = 0;
	}
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

void GL3SceneGraphRenderer::render(Node* node, Camera* camera) {
	if (nullptr == node || nullptr == camera || shader_program == 0)
		return;
	glUseProgram(shader_program);
	walk_init_buffers(node);
	glClearColor(0.55f, 0.68f, 0.82f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	if (uniform_transform_buffer_id != 0) {
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_transform_buffer_id);
		GLint projection_offset = 0;
		GLint modelview_offset = sizeof(glm::mat4);
		glBufferSubData(GL_UNIFORM_BUFFER, projection_offset, sizeof(glm::mat4),
				glm::value_ptr(camera->projection_matrix));
		glBufferSubData(GL_UNIFORM_BUFFER, modelview_offset, sizeof(glm::mat4),
				glm::value_ptr(camera->modelview_matrix));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	GLint matrix_loc = static_cast<GLint>(matrix_uniform_location);
	if (matrix_loc >= 0) {
		glm::mat4 identity(1.0f);
		glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, glm::value_ptr(identity));
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, default_texture_id);
	GLint tex_loc = glGetUniformLocation(shader_program, "diffuseTexture");
	if (tex_loc >= 0) {
		glUniform1i(tex_loc, 0);
	}

	walk_render(node);

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);
}
