// Copyright (C) 2017 Chris Liebert

#ifndef _GL3_RENDERER_H_
#define _GL3_RENDERER_H_

using namespace scenegraph;

class GL3SceneGraphRenderer {
protected:
	GLuint shader_program;
	GLuint uniform_transform_buffer_id, binding_point_index, transform_block_id;
	GLint uniform_transform_buffer_block_size;
	std::map<GeometryNode*, GLuint> vaos;
	std::map<GeometryNode*, GLuint> vbos;
	std::map<std::string, GLuint> texture_ids;
	GLuint matrix_uniform_location;

	void walk_init_buffers(Node* node);
	void walk_render(Node* node);
public:
	GL3SceneGraphRenderer(std::map<std::string, Image*>& texture_names);
	~GL3SceneGraphRenderer();
	void render(Node* node, Camera* camera);
};

#endif // _GL3_RENDERER_H_
