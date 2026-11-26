// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/bullet_debug_drawer.h"
#include "common/log.h"
#include <iostream>

BulletDebugDrawer::BulletDebugDrawer()
    : vao(0),
      vbo(0),
      shader_program(0),
      mvp_location(-1),
      debug_mode(DBG_NoDebug),
      buffers_initialized(false) {}

BulletDebugDrawer::~BulletDebugDrawer() {
    cleanUp();
}

void BulletDebugDrawer::cleanUp() {
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (shader_program != 0) {
        glDeleteProgram(shader_program);
        shader_program = 0;
    }
    buffers_initialized = false;
}

void BulletDebugDrawer::initGL() {
    if (buffers_initialized) return;

    // Line drawing vertex & fragment shaders
    const char* vert_src =
        "#version 300 es\n"
        "layout(location = 0) in vec3 vPosition;\n"
        "layout(location = 1) in vec3 vColor;\n"
        "uniform mat4 mvp;\n"
        "out vec3 fragColor;\n"
        "void main() {\n"
        "    gl_Position = mvp * vec4(vPosition, 1.0);\n"
        "    fragColor = vColor;\n"
        "}\n";

    const char* frag_src =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec3 fragColor;\n"
        "out vec4 outColor;\n"
        "void main() {\n"
        "    outColor = vec4(fragColor, 1.0);\n"
        "}\n";

    shader_program = createProgram(vert_src, frag_src);
    if (!shader_program) {
        LOGE("Failed to create Bullet line debug shader program!");
        return;
    }
    mvp_location = glGetUniformLocation(shader_program, "mvp");

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Position attribute (layout location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);

    // Color attribute (layout location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    buffers_initialized = true;
    LOGI("BulletDebugDrawer initialized GL buffers (VAO: %u, VBO: %u, Program: %u)",
         vao, vbo, shader_program);
}

void BulletDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color) {
    line_buffer.push_back({
        static_cast<float>(from.x()), static_cast<float>(from.y()), static_cast<float>(from.z()),
        static_cast<float>(color.x()), static_cast<float>(color.y()), static_cast<float>(color.z())
    });
    line_buffer.push_back({
        static_cast<float>(to.x()), static_cast<float>(to.y()), static_cast<float>(to.z()),
        static_cast<float>(color.x()), static_cast<float>(color.y()), static_cast<float>(color.z())
    });
}

void BulletDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& fromColor, const btVector3& toColor) {
    line_buffer.push_back({
        static_cast<float>(from.x()), static_cast<float>(from.y()), static_cast<float>(from.z()),
        static_cast<float>(fromColor.x()), static_cast<float>(fromColor.y()), static_cast<float>(fromColor.z())
    });
    line_buffer.push_back({
        static_cast<float>(to.x()), static_cast<float>(to.y()), static_cast<float>(to.z()),
        static_cast<float>(toColor.x()), static_cast<float>(toColor.y()), static_cast<float>(toColor.z())
    });
}

void BulletDebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) {
    (void)lifeTime;
    btScalar normal_len = distance > 0.01f ? distance : 0.4f;
    btVector3 to = PointOnB + normalOnB * normal_len;
    drawLine(PointOnB, to, color);

    // Draw small cross at contact point for high visibility
    const btScalar cross_size = 0.08f;
    drawLine(PointOnB - btVector3(cross_size, 0, 0), PointOnB + btVector3(cross_size, 0, 0), btVector3(1.0f, 0.2f, 0.2f));
    drawLine(PointOnB - btVector3(0, cross_size, 0), PointOnB + btVector3(0, cross_size, 0), btVector3(1.0f, 0.2f, 0.2f));
    drawLine(PointOnB - btVector3(0, 0, cross_size), PointOnB + btVector3(0, 0, cross_size), btVector3(1.0f, 0.2f, 0.2f));
}

void BulletDebugDrawer::reportErrorWarning(const char* warningString) {
    LOGI("Bullet Physics Warning: %s", warningString ? warningString : "unknown");
}

void BulletDebugDrawer::draw3dText(const btVector3& location, const char* textString) {
    (void)location;
    (void)textString;
}

void BulletDebugDrawer::setDebugMode(int debugMode) {
    debug_mode = debugMode;
}

int BulletDebugDrawer::getDebugMode() const {
    return debug_mode;
}

void BulletDebugDrawer::render(Camera* camera) {
    if (line_buffer.empty() || !camera) return;

    if (!buffers_initialized) {
        initGL();
        if (!buffers_initialized) return;
    }

    glm::mat4 mvp = camera->projection_matrix * camera->modelview_matrix;

    glUseProgram(shader_program);
    if (mvp_location >= 0) {
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(mvp));
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, line_buffer.size() * sizeof(LineVertex), line_buffer.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(line_buffer.size()));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void BulletDebugDrawer::clear() {
    line_buffer.clear();
}
