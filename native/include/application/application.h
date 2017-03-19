#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <cassert>

#include "graphics/camera.h"
#include "graphics/gl_code.h"
#include "graphics/scene_graph.h"
#include "graphics/wavefront_factory.h"

#include "physics/simulation.h"

typedef struct Application {
    float grey;
    Simulation* simulation;
    scenegraph::Node* scenegraph_root;
    Camera* camera;
    std::map<std::string, Image*> images;



#ifndef DESKTOP_APP
    AAssetManager* asset_manager;
    Application(AAssetManager* asset_manager);
#else
    Application();
#endif
    ~Application();
    Node* loadResources();

    void init();
    void step();

    template<typename SceneGraphRenderer_T>
    void render(SceneGraphRenderer_T* renderer) {
    	renderer->render(scenegraph_root, camera);
    }

    void resize(int width, int height);
} Application;

#endif //_APPLICATION_H_
