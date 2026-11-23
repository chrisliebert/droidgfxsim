// Copyright (C) 2017-2026 Chris Liebert

#ifndef _SHADOW_MAP_H_
#define _SHADOW_MAP_H_

#include "graphics/gl_code.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class ShadowMap {
private:
    GLuint depth_fbo = 0;
    GLuint depth_texture = 0;
    GLuint shadow_shader_program = 0;
    GLint light_space_matrix_location = -1;
    GLint model_matrix_location = -1;

    int shadow_width = 2048;
    int shadow_height = 2048;
    GLint prev_viewport[4] = {0, 0, 0, 0};
    bool initialized = false;

    glm::mat4 light_space_matrix = glm::mat4(1.0f);
    glm::vec3 current_light_pos = glm::vec3(20.0f, 40.0f, 20.0f);

public:
    ShadowMap();
    ~ShadowMap();

    // Initialize FBO, 2048x2048 depth texture, and shadow shader
    bool init(int width = 2048, int height = 2048);
    void cleanUp();

    // Static helper to compute light-space matrix (projection * view)
    static glm::mat4 computeLightSpaceMatrix(
        const glm::vec3& lightPos,
        const glm::vec3& targetPos = glm::vec3(0.0f, 0.0f, 0.0f),
        const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
        float orthoSize = 30.0f,
        float nearPlane = 1.0f,
        float farPlane = 100.0f);

    // Update light position and recalculate light space matrix
    void updateLightSpaceMatrix(
        const glm::vec3& lightPos,
        const glm::vec3& targetPos = glm::vec3(0.0f, 0.0f, 0.0f),
        const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
        float orthoSize = 30.0f,
        float nearPlane = 1.0f,
        float farPlane = 100.0f);

    // Prepare FBO and state for rendering shadow depth pass
    void beginDepthPass(
        const glm::vec3& lightPos,
        const glm::vec3& targetPos = glm::vec3(0.0f, 0.0f, 0.0f),
        float orthoSize = 30.0f,
        float nearPlane = 1.0f,
        float farPlane = 100.0f);

    // Begin depth pass using currently configured light space matrix
    void beginDepthPass();

    // Finish depth pass and restore previous viewport and back-face culling
    void endDepthPass(int restore_viewport_w = 0, int restore_viewport_h = 0);

    // Bind depth texture to texture unit (defaults to GL_TEXTURE1)
    void bindDepthTexture(GLenum texture_unit = GL_TEXTURE1) const;

    // Unbind depth texture from texture unit
    void unbindDepthTexture(GLenum texture_unit = GL_TEXTURE1) const;

    // Set model matrix for node rendering in shadow shader
    void setModelMatrix(const glm::mat4& model);

    // Accessors
    GLuint getDepthFBO() const { return depth_fbo; }
    GLuint getDepthTexture() const { return depth_texture; }
    GLuint getShadowProgram() const { return shadow_shader_program; }
    int getShadowWidth() const { return shadow_width; }
    int getShadowHeight() const { return shadow_height; }
    const glm::mat4& getLightSpaceMatrix() const { return light_space_matrix; }
    const glm::vec3& getLightPos() const { return current_light_pos; }
    bool isInitialized() const { return initialized; }
};

#endif // _SHADOW_MAP_H_
