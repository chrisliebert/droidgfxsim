#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "graphics/gl3_renderer.h"

void GL3SceneGraphRenderer::walk_init_buffers(Node* node) {
	if(node == 0) return;
	if(node->type == NodeType::Geometry) {
		GeometryNode* geometry_node = (GeometryNode*) node;
		assert(geometry_node);
		if(vbos.find(geometry_node) == vbos.end()) {
			//LOGI("Creating new geometry node %s", geometry_node->name.c_str());
			GLuint vao, vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * geometry_node->vertex_data.size(),
					geometry_node->vertex_data.data(), GL_STATIC_DRAW);

			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
            vaos.insert(std::make_pair(geometry_node, vao));

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			//glGenBuffers(OBJECTS_COUNT, vinx);
			//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[i]);
			//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes[0]) * faces_count[i] * 3,
			//			 &indexes[indices_offset_table[i]], GL_STATIC_DRAW);
			//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			vbos.insert(std::make_pair(geometry_node, vbo));
		}
	}
	for(std::vector<Node*>::iterator it = node->children.begin(); it!=node->children.end(); ++it) {
		Node* child = *it;
		walk_init_buffers(child);
	}
}

void GL3SceneGraphRenderer::walk_render(Node* node) {
	if(node == 0) return;
	if(node->type == NodeType::Geometry) {
		GeometryNode* geometry_node = (GeometryNode*) node;
		assert(geometry_node);
		if(vbos.find(geometry_node) != vbos.end()) {
			GLuint vbo = vbos[geometry_node];
            std::map<GeometryNode*, GLuint>::iterator vao_node = vaos.find(geometry_node);
            if(vao_node->first != geometry_node) {
                LOGE("vao not found for vbo node in %s", geometry_node->name.c_str());
                exit(8);
            }
            GLuint vao = vao_node->second;
            glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const GLvoid*)offsetof(Vertex, position));

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(0));

            glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const GLvoid*)offsetof(Vertex, normal));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),	BUFFER_OFFSET(3 * sizeof(float)));

            glVertexAttribPointer(2, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const GLvoid*)offsetof(Vertex, textureCoordinate));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),	BUFFER_OFFSET(6 * sizeof(float)));

            glDrawArrays(GL_TRIANGLES, 0, geometry_node->vertex_data.size());
            //glDrawElements(GL_TRIANGLES, faces_count[index] * 3, INX_TYPE, BUFFER_OFFSET(0));
            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
		}
	} else if(node->type == NodeType::Material) {
		MaterialNode* material_node = (MaterialNode*) node;
		assert(material_node);
		std::map<std::string, GLuint>::iterator texture_id_itr = texture_ids.find(material_node->diffuse_texture);
		if(texture_id_itr == texture_ids.end()) {
			//LOGE("Unable to use texture %s", material_node->diffuse_texture.c_str());
		} else {
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

GL3SceneGraphRenderer::GL3SceneGraphRenderer(std::map<std::string, Image*>& images) {
	for(std::map<std::string, Image*>::iterator it = images.begin(); it != images.end(); ++it) {
		texture_ids[it->first] = loadTexture(it->second);
		if(it->second->data) {
			delete it->second->data;
			it->second->data = 0;
		}
		delete it->second;
		it->second = 0;
	}
	images.clear();

	const char* vertex_shader_src =
		"#version 300 es                            										\n"
		"layout(location = 0) in vec3 vPosition;					        	    		\n"
		"layout(location = 1) in vec3 vNormal;												\n"
		"layout(location = 2) in vec2 vTexCoord;											\n"
		"layout (std140) uniform TransformBlock {											\n"
		"	mat4 projection;               													\n"
		"	mat4 modelview;               													\n"
		"};																					\n"
		"uniform mat4 matrix;																\n"
		"out vec3 fragPos;																	\n"
		"out vec3 normal;																	\n"
		"out vec2 texcoord;																	\n"
		"out vec3 lightPos;																	\n"
		"void main() {																		\n"
		"	gl_Position = projection * modelview											\n"
		"		* matrix * vec4(vPosition, 1.0);											\n"
		"	fragPos = vec3(modelview * matrix * vec4(vPosition, 1.0f));						\n"
		"	normal = mat3(transpose(inverse(modelview * matrix))) * vNormal;				\n"
		"	vec3 lightPosIn = vec3(0.0, 10.0, 0.0);											\n"
		"	lightPos = vec3(modelview * vec4(lightPosIn, 1.0));								\n"
		"	texcoord = vTexCoord;															\n"
		"}																					\n";

	const char* fragment_shader_src =
		"#version 300 es																	\n"
		"precision mediump float;                           			      	 			\n"
		"in vec3 fragPos;																	\n"
		"in vec3 normal;																	\n"
		"in vec2 texcoord;																	\n"
		"in vec3 lightPos;																	\n"
		"out vec4 color;																	\n"
		"//uniform vec3 lightColor;															\n"
		"//uniform vec3 objectColor;														\n"
		"uniform sampler2D diffuseTexture;													\n"
		"void main()																		\n"
		"{																					\n"
		"	vec3 lightColor = vec3(0.5, 0.5, 0.5);											\n"
		"	vec3 objectColor = texture(diffuseTexture, texcoord).rgb;						\n"
		"	float ambientStrength = 0.1f;													\n"
		"	vec3 ambient = ambientStrength * lightColor;    								\n"
		"	vec3 norm = normalize(normal);													\n"
		"	vec3 lightDir = normalize(lightPos - fragPos);									\n"
		"	float diff = max(dot(norm, lightDir), 0.0);									\n"
		"	vec3 diffuse = diff * lightColor;												\n"
		"	float specularStrength = 0.5f;													\n"
		"	vec3 viewDir = normalize(-fragPos);											\n"
		"	vec3 reflectDir = reflect(-lightDir, norm);  									\n"
		"	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.f);					\n"
		"	vec3 specular = specularStrength * spec * lightColor; 							\n"
		"	vec3 result = (ambient + diffuse + specular) * objectColor;					\n"
		"	color = vec4(result, 1.0f);													\n"
		"}																					\n";
	shader_program = createProgram(vertex_shader_src, fragment_shader_src);
	glUseProgram(shader_program);
	glActiveTexture(GL_TEXTURE0);
	matrix_uniform_location = glGetUniformLocation(shader_program, "matrix");
	binding_point_index = 1;
	// Retrieve the uniform block index
	transform_block_id = glGetUniformBlockIndex (shader_program, "TransformBlock");
	// Associate the uniform block index with a binding point
	glUniformBlockBinding (shader_program, transform_block_id, binding_point_index);
	glGetActiveUniformBlockiv (shader_program, transform_block_id, GL_UNIFORM_BLOCK_DATA_SIZE, &uniform_transform_buffer_block_size);
	// Create and fill a buffer object
	glGenBuffers (1, &uniform_transform_buffer_id);
	glBindBuffer (GL_UNIFORM_BUFFER, uniform_transform_buffer_id);
	glBufferData (GL_UNIFORM_BUFFER, uniform_transform_buffer_block_size, (void*)0, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, binding_point_index, uniform_transform_buffer_id, 0, 2 * 16 * sizeof(float));
	glUseProgram(0);
}

GL3SceneGraphRenderer::~GL3SceneGraphRenderer() {
    for(std::map<GeometryNode*, GLuint>::iterator it = vaos.begin(); it != vaos.end(); ++it) {
        GLuint vao = it->second;
        glDeleteVertexArrays(1, &vao);
    }

	for(std::map<GeometryNode*, GLuint>::iterator it = vbos.begin(); it != vbos.end(); ++it) {
		GLuint vbo = it->second;
		glDeleteBuffers(1, &vbo);
	}
	glDeleteBuffers(1, &uniform_transform_buffer_id);
	for(std::map<std::string, GLuint>::iterator it = texture_ids.begin(); it != texture_ids.end(); ++it) {
		GLuint texture_id = it->second;
		glDeleteTextures(1, &texture_id);
	}

	glDeleteProgram(shader_program);
	vaos.clear();
	vbos.clear();
}

#include "graphics/stb_easy_font.h"

void printString(float x, float y, char *text, float r, float g, float b)
{
	//TODO: use custom shader
	static char buffer[99999]; // ~500 chars
	int num_quads;
	GLuint vertex_data_size = sizeof(float) * 4;

	num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));


	GLuint vao, vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_data_size, buffer, GL_STATIC_DRAW);
	glGenVertexArrays(1, &vao);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex_data_size, BUFFER_OFFSET(0));


	glDrawArrays(GL_POINTS, 0, num_quads * 4);
	//glDrawElements(GL_TRIANGLES, faces_count[index] * 3, INX_TYPE, BUFFER_OFFSET(0));
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	//glColor3f(r,g,b);
	//glEnableClientState(GL_VERTEX_ARRAY);
	//glVertexPointer(2, GL_FLOAT, 16, buffer);
	//glDrawArrays(GL_QUADS, 0, num_quads*4);
	//glDisableClientState(GL_VERTEX_ARRAY);
}


void GL3SceneGraphRenderer::render(Node* node, Camera* camera) {
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shader_program);

	walk_init_buffers(node);

	/* Alternate version not supported with Android, or requires extension loading
	glBindBuffer(GL_UNIFORM_BUFFER, uniform_transform_buffer_id);
	GLvoid* p1 = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
	GLsizeiptr matrix_size = 16 * sizeof(float);
	GLvoid* p2 = (char*)p1 + matrix_size;
	memcpy(p1, glm::value_ptr(camera->projectionMatrix), matrix_size);
	memcpy(p2, glm::value_ptr(camera->modelViewMatrix), matrix_size);
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	*/

	GLsizeiptr matrix_size = 16 * sizeof(float);
	glBindBuffer(GL_UNIFORM_BUFFER, uniform_transform_buffer_id);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, matrix_size, glm::value_ptr(camera->projectionMatrix));
	//glBindBuffer(GL_UNIFORM_BUFFER, 0);

	//glBindBuffer(GL_UNIFORM_BUFFER, uniform_transform_buffer_id);
	glBufferSubData(GL_UNIFORM_BUFFER, matrix_size, matrix_size, glm::value_ptr(camera->modelViewMatrix));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	walk_render(node);

	glUseProgram(0);
}
