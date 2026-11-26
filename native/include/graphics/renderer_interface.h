#ifndef _RENDERER_INTERFACE_H_
#define _RENDERER_INTERFACE_H_

namespace scenegraph {
    class Node;
}

class Camera;

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(scenegraph::Node* node, Camera* camera) = 0;
};

#endif // _RENDERER_INTERFACE_H_
