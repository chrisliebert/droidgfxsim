#ifndef _GL2_RENDERER_H_
#define _GL2_RENDERER_H_

using namespace scenegraph;

class GL2SceneGraphRenderer {
protected:
	GLuint shader_program;
	std::map<GeometryNode*, GLuint> vbos;
	//std::map<GeometryNode*, GLuint> ibos;
	std::map<std::string, GLuint> texture_ids;
	GLuint matrix_uniform_location;

	void walk_init_buffers(Node* node);
  void walk_render(Node* node);
public:
	GL2SceneGraphRenderer(std::map<std::string, Image*>& images);
	~GL2SceneGraphRenderer();
	void render(Node* node, Camera* camera);
};

#endif //_GL2_RENDERER_H_
