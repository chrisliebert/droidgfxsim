// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/shadow_map.h"
#include "common/log.h"
#include <iostream>

ShadowMap::ShadowMap()
    : depth_fbo(0),
      depth_texture(0),
      shadow_shader_program(0),
      light_space_matrix_location(-1),
      model_matrix_location(-1),
      shadow_width(2048),
      shadow_height(2048),
      initialized(false),
      light_space_matrix(1.0f),
      current_light_pos(20.0f, 40.0f, 20.0f) {
    updateLightSpaceMatrix(current_light_pos);
}

ShadowMap::~ShadowMap() {
    cleanUp();
}

void ShadowMap::cleanUp() {
    if (depth_fbo != 0) {
        glDeleteFramebuffers(1, &depth_fbo);
        depth_fbo = 0;
    }
    if (depth_texture != 0) {
        glDeleteTextures(1, &depth_texture);
        depth_texture = 0;
    }
    if (shadow_shader_program != 0) {
        glDeleteProgram(shadow_shader_program);
        shadow_shader_program = 0;
    }
    initialized = false;
}

glm::mat4 ShadowMap::computeLightSpaceMatrix(
    const glm::vec3& lightPos,
    const glm::vec3& targetPos,
    const glm::vec3& up,
    float orthoSize,
    float nearPlane,
    float farPlane)
{
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
    glm::mat4 lightView = glm::lookAt(lightPos, targetPos, up);
    return lightProjection * lightView;
}

void ShadowMap::updateLightSpaceMatrix(
    const glm::vec3& lightPos,
    const glm::vec3& targetPos,
    const glm::vec3& up,
    float orthoSize,
    float nearPlane,
    float farPlane)
{
    current_light_pos = lightPos;
    light_space_matrix = computeLightSpaceMatrix(lightPos, targetPos, up, orthoSize, nearPlane, farPlane);
}

bool ShadowMap::init(int width, int height) {
    cleanUp();
    shadow_width = width;
    shadow_height = height;

    // 1. Create depth texture
    glGenTextures(1, &depth_texture);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_width, shadow_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(GL_CLAMP_TO_BORDER) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !defined(TARGET_OS_IPHONE)
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
#endif

    // 2. Create depth FBO
    glGenFramebuffers(1, &depth_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__) && !defined(TARGET_OS_IPHONE)
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
#endif

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Shadow FBO creation incomplete! Status code: 0x%x", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        cleanUp();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 3. Compile depth-pass shader
    const char* shadow_vert =
        "#version 300 es\n"
        "layout(location = 0) in vec3 vPosition;\n"
        "uniform mat4 lightSpaceMatrix;\n"
        "uniform mat4 matrix;\n"
        "void main() {\n"
        "    gl_Position = lightSpaceMatrix * matrix * vec4(vPosition, 1.0);\n"
        "}\n";

    const char* shadow_frag =
        "#version 300 es\n"
        "precision highp float;\n"
        "void main() {}\n";

    shadow_shader_program = createProgram(shadow_vert, shadow_frag);
    if (!shadow_shader_program) {
        LOGE("Failed to compile shadow depth pass shader!");
        cleanUp();
        return false;
    }

    light_space_matrix_location = glGetUniformLocation(shadow_shader_program, "lightSpaceMatrix");
    model_matrix_location = glGetUniformLocation(shadow_shader_program, "matrix");

    initialized = true;
    LOGI("ShadowMap initialized successfully (%dx%d, FBO: %u, Texture: %u, Program: %u)",
         shadow_width, shadow_height, depth_fbo, depth_texture, shadow_shader_program);
    return true;
}

void ShadowMap::beginDepthPass(
    const glm::vec3& lightPos,
    const glm::vec3& targetPos,
    float orthoSize,
    float nearPlane,
    float farPlane)
{
    updateLightSpaceMatrix(lightPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f), orthoSize, nearPlane, farPlane);
    beginDepthPass();
}

void ShadowMap::beginDepthPass() {
    if (!initialized) {
        init(shadow_width, shadow_height);
    }

    glGetIntegerv(GL_VIEWPORT, prev_viewport);
    glViewport(0, 0, shadow_width, shadow_height);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // Cull front faces to eliminate shadow acne

    glUseProgram(shadow_shader_program);
    if (light_space_matrix_location >= 0) {
        glUniformMatrix4fv(light_space_matrix_location, 1, GL_FALSE, glm::value_ptr(light_space_matrix));
    }
}

void ShadowMap::endDepthPass(int restore_viewport_w, int restore_viewport_h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glCullFace(GL_BACK); // Restore back-face culling for normal render pass
    glUseProgram(0);

    if (restore_viewport_w > 0 && restore_viewport_h > 0) {
        glViewport(0, 0, restore_viewport_w, restore_viewport_h);
    } else {
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    }
}

void ShadowMap::bindDepthTexture(GLenum texture_unit) const {
    if (depth_texture != 0) {
        glActiveTexture(texture_unit);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
    }
}

void ShadowMap::unbindDepthTexture(GLenum texture_unit) const {
    glActiveTexture(texture_unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ShadowMap::setModelMatrix(const glm::mat4& model) {
    if (shadow_shader_program != 0 && model_matrix_location >= 0) {
        glUniformMatrix4fv(model_matrix_location, 1, GL_FALSE, glm::value_ptr(model));
    }
}
