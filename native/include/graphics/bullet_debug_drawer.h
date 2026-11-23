// Copyright (C) 2017-2026 Chris Liebert

#ifndef _BULLET_DEBUG_DRAWER_H_
#define _BULLET_DEBUG_DRAWER_H_

#include "LinearMath/btIDebugDraw.h"
#include "graphics/gl_code.h"
#include "graphics/camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

class BulletDebugDrawer : public btIDebugDraw {
public:
    struct LineVertex {
        float x, y, z;
        float r, g, b;
    };

private:
    std::vector<LineVertex> line_buffer;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shader_program = 0;
    GLint mvp_location = -1;
    int debug_mode = DBG_NoDebug;
    bool buffers_initialized = false;

    void initGL();
    void cleanUp();

public:
    BulletDebugDrawer();
    virtual ~BulletDebugDrawer() override;

    // btIDebugDraw interface implementations
    virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
    virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& fromColor, const btVector3& toColor) override;
    virtual void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override;
    virtual void reportErrorWarning(const char* warningString) override;
    virtual void draw3dText(const btVector3& location, const char* textString) override;
    virtual void setDebugMode(int debugMode) override;
    virtual int getDebugMode() const override;

    // Render accumulated lines using camera view-projection matrix
    void render(Camera* camera);

    // Clear line vertex buffer for next frame
    void clear();

    // Query line vertex count
    size_t getLineCount() const { return line_buffer.size() / 2; }
    size_t getVertexCount() const { return line_buffer.size(); }
    const std::vector<LineVertex>& getVertices() const { return line_buffer; }
};

#endif // _BULLET_DEBUG_DRAWER_H_
