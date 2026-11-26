// Copyright (C) 2017-2026 Chris Liebert

#ifndef _GL2_RENDERER_H_
#define _GL2_RENDERER_H_

#include "graphics/renderer_interface.h"
#include "graphics/scene_graph.h"
#include <map>
#include <string>

using namespace scenegraph;

class GL2SceneGraphRenderer : public IRenderer {
protected:
	GLuint shader_program;
	std::map<GeometryNode*, GLuint> vbos;
	std::map<GeometryNode*, GLuint> ibos;
	std::map<std::string, GLuint> texture_ids;
	GLuint matrix_uniform_location;
	GLuint default_texture_id;

	void walk_init_buffers(Node* node);
	void walk_render(Node* node);
public:
	GL2SceneGraphRenderer(std::map<std::string, Image*>& images);
	virtual ~GL2SceneGraphRenderer() override;
	virtual void render(Node* node, Camera* camera) override;
};

#endif //_GL2_RENDERER_H_
