// Copyright (C) 2017-2026 Chris Liebert

#ifndef _GL3_RENDERER_H_
#define _GL3_RENDERER_H_

#include "graphics/renderer_interface.h"
#include "graphics/scene_graph.h"
#include <map>
#include <string>

using namespace scenegraph;

class GL3SceneGraphRenderer : public IRenderer {
protected:
	GLuint shader_program;
	GLuint uniform_transform_buffer_id, binding_point_index, transform_block_id;
	GLint uniform_transform_buffer_block_size;
	std::map<GeometryNode*, GLuint> vaos;
	std::map<GeometryNode*, GLuint> vbos;
	std::map<GeometryNode*, GLuint> ibos;
	std::map<std::string, GLuint> texture_ids;
	GLuint matrix_uniform_location;
	GLuint default_texture_id;

	void walk_init_buffers(Node* node);
	void walk_render(Node* node);
public:
	GL3SceneGraphRenderer(std::map<std::string, Image*>& texture_names);
	virtual ~GL3SceneGraphRenderer() override;
	virtual void render(Node* node, Camera* camera) override;
};

#endif // _GL3_RENDERER_H_
