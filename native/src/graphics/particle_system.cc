// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/particle_system.h"
#include "graphics/gl_code.h"
#include "common/log.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ParticleSystem::ParticleSystem() {
    init(5000);
}

ParticleSystem::~ParticleSystem() {
    cleanupGL();
}

void ParticleSystem::init(size_t max_particles) {
    max_capacity_ = max_particles;
    particles_.resize(max_capacity_);
    vertex_buffer_.resize(max_capacity_);
    active_count_ = 0;

    if (gl_initialized_ && vbo_ != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, max_capacity_ * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ParticleSystem::clear() {
    active_count_ = 0;
}

size_t ParticleSystem::getActiveCount() const {
    return active_count_;
}

void ParticleSystem::emitBurst(const glm::vec3& position, const glm::vec3& normal, int count, const glm::vec4& color) {
    if (count <= 0 || max_capacity_ == 0) return;

    glm::vec3 n = normal;
    if (glm::length(n) < 0.001f) {
        n = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        n = glm::normalize(n);
    }

    // Build orthonormal coordinate system around normal vector
    glm::vec3 up = (std::abs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, n));
    glm::vec3 bitangent = glm::cross(n, tangent);

    for (int i = 0; i < count; ++i) {
        if (active_count_ >= max_capacity_) {
            break; // Capacity reached
        }

        // Random azimuthal angle [0, 2*pi)
        float phi = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f * static_cast<float>(M_PI);
        // Cosine-weighted distribution for natural cone dispersion
        float cos_theta = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));

        // Perturbed direction vector
        glm::vec3 dir = tangent * (std::cos(phi) * sin_theta) +
                        bitangent * (std::sin(phi) * sin_theta) +
                        n * (0.35f + 0.65f * cos_theta);
        if (glm::length(dir) > 0.0001f) {
            dir = glm::normalize(dir);
        } else {
            dir = n;
        }

        float speed = 7.0f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 18.0f; // 7 to 25 m/s
        float life = 0.35f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.85f;  // 0.35 to 1.2s
        float size = 0.05f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.10f; // 0.05 to 0.15

        // Slight spark tint variation (orange / gold / incandescent)
        float r_var = 0.85f + 0.25f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
        float g_var = 0.75f + 0.30f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
        glm::vec4 spark_col(
            std::clamp(color.r * r_var, 0.0f, 1.0f),
            std::clamp(color.g * g_var, 0.0f, 1.0f),
            std::clamp(color.b * 0.7f, 0.0f, 1.0f),
            color.a
        );

        Particle& p = particles_[active_count_++];
        p.position = position + dir * 0.05f;
        p.velocity = dir * speed;
        p.color = spark_col;
        p.life = life;
        p.max_life = life;
        p.size = size;
        p.gravity = -12.0f;
        p.drag = 0.98f;
        p.size_growth = 0.0f;
    }
}

void ParticleSystem::emitDebrisBurst(const glm::vec3& position, const glm::vec3& normal, int count, int burst_type) {
    if (count <= 0 || max_capacity_ == 0) return;

    glm::vec3 n = normal;
    if (glm::length(n) < 0.001f) {
        n = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        n = glm::normalize(n);
    }

    // Build orthonormal coordinate system around normal vector
    glm::vec3 up = (std::abs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, n));
    glm::vec3 bitangent = glm::cross(n, tangent);

    for (int i = 0; i < count; ++i) {
        if (active_count_ >= max_capacity_) {
            break; // Capacity reached
        }

        // Random azimuthal angle [0, 2*pi)
        float phi = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f * static_cast<float>(M_PI);
        float cos_theta = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));

        Particle& p = particles_[active_count_++];

        if (burst_type == 1) {
            // Type 1 (Dust / Smoke Puffs): Grey / white soft expanding smoke puffs with gentle buoyancy and high drag
            glm::vec3 dir = tangent * (std::cos(phi) * sin_theta * 0.8f) +
                            bitangent * (std::sin(phi) * sin_theta * 0.8f) +
                            n * (0.25f + 0.75f * cos_theta);
            if (glm::length(dir) > 0.0001f) {
                dir = glm::normalize(dir);
            } else {
                dir = n;
            }

            float speed = 2.0f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 4.5f;   // 2 to 6.5 m/s
            float life = 1.0f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 1.5f;    // 1.0 to 2.5s
            float size = 0.12f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.12f;  // 0.12 to 0.24 initial size

            float shade = 0.65f + 0.30f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)); // 0.65 to 0.95
            float alpha = 0.25f + 0.20f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)); // 0.25 to 0.45 soft opacity
            glm::vec4 smoke_col(shade * 0.95f, shade * 0.95f, shade, alpha);

            p.position = position + dir * 0.08f;
            p.velocity = dir * speed;
            p.color = smoke_col;
            p.life = life;
            p.max_life = life;
            p.size = size;
            p.gravity = 1.2f;    // Gentle buoyancy upward
            p.drag = 0.88f;       // High aerodynamic drag
            p.size_growth = 0.22f; // Expanding smoke puff
        } else if (burst_type == 2) {
            // Type 2 (Plasma / Cannonball Impact): Cyan / violet / white high-velocity shockwave sparks
            glm::vec3 dir = tangent * (std::cos(phi) * sin_theta) +
                            bitangent * (std::sin(phi) * sin_theta) +
                            n * (0.15f + 0.85f * cos_theta);
            if (glm::length(dir) > 0.0001f) {
                dir = glm::normalize(dir);
            } else {
                dir = n;
            }

            float speed = 14.0f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 24.0f; // 14 to 38 m/s
            float life = 0.25f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.60f;  // 0.25 to 0.85s
            float size = 0.08f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.14f;  // 0.08 to 0.22

            float c_pick = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            glm::vec4 plasma_col;
            if (c_pick < 0.40f) {
                // Electric cyan / neon teal
                plasma_col = glm::vec4(0.15f, 0.92f, 1.0f, 1.0f);
            } else if (c_pick < 0.75f) {
                // Vivid violet / purple / magenta
                plasma_col = glm::vec4(0.85f, 0.25f, 1.0f, 1.0f);
            } else {
                // Incandescent core white
                plasma_col = glm::vec4(0.95f, 0.98f, 1.0f, 1.0f);
            }

            p.position = position + dir * 0.06f;
            p.velocity = dir * speed;
            p.color = plasma_col;
            p.life = life;
            p.max_life = life;
            p.size = size;
            p.gravity = -7.5f;
            p.drag = 0.96f;
            p.size_growth = -0.02f;
        } else {
            // Type 0 (Sparks, default): Gold / orange / yellow high-velocity sparks with gravity (g = -12.0) and drag
            glm::vec3 dir = tangent * (std::cos(phi) * sin_theta) +
                            bitangent * (std::sin(phi) * sin_theta) +
                            n * (0.35f + 0.65f * cos_theta);
            if (glm::length(dir) > 0.0001f) {
                dir = glm::normalize(dir);
            } else {
                dir = n;
            }

            float speed = 9.0f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 20.0f;  // 9 to 29 m/s
            float life = 0.35f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.75f;  // 0.35 to 1.1s
            float size = 0.05f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.09f;  // 0.05 to 0.14

            float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            glm::vec4 spark_col;
            if (t < 0.40f) {
                spark_col = glm::vec4(1.0f, 0.82f, 0.18f, 1.0f); // Gold
            } else if (t < 0.75f) {
                spark_col = glm::vec4(1.0f, 0.52f, 0.08f, 1.0f); // Orange
            } else {
                spark_col = glm::vec4(1.0f, 0.96f, 0.38f, 1.0f); // Yellow
            }
            float tint = 0.9f + 0.2f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
            spark_col.r = std::clamp(spark_col.r * tint, 0.0f, 1.0f);
            spark_col.g = std::clamp(spark_col.g * tint, 0.0f, 1.0f);
            spark_col.b = std::clamp(spark_col.b * tint, 0.0f, 1.0f);

            p.position = position + dir * 0.05f;
            p.velocity = dir * speed;
            p.color = spark_col;
            p.life = life;
            p.max_life = life;
            p.size = size;
            p.gravity = -12.0f;
            p.drag = 0.98f;
            p.size_growth = 0.0f;
        }
    }
}

void ParticleSystem::update(float dt) {
    if (dt <= 0.0f || active_count_ == 0) return;

    float last_drag = -1.0f;
    float last_drag_factor = 1.0f;

    for (size_t i = 0; i < active_count_; ) {
        Particle& p = particles_[i];
        p.life -= dt;

        if (p.life <= 0.0f) {
            // Swap with back of active set (O(1) compaction)
            particles_[i] = particles_[active_count_ - 1];
            active_count_--;
            continue;
        }

        // Velocity integration: p <- p + v * dt
        p.position += p.velocity * dt;

        // Gravity / buoyancy: v_y <- v_y + gravity * dt
        p.velocity.y += p.gravity * dt;

        // Drag: v <- v * (drag^(dt * 60))
        if (p.drag != last_drag) {
            last_drag = p.drag;
            last_drag_factor = std::pow(p.drag, dt * 60.0f);
        }
        p.velocity *= last_drag_factor;

        // Size expansion / decay
        p.size = std::max(0.01f, p.size + p.size_growth * dt);

        // Alpha decay over lifetime
        float life_ratio = (p.max_life > 0.0001f) ? (p.life / p.max_life) : 0.0f;
        p.color.a = std::clamp(life_ratio, 0.0f, 1.0f);

        i++;
    }
}

void ParticleSystem::initGL() {
    if (gl_initialized_) return;

    const char* vert_src_gl3 =
        "#version 300 es\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 1) in vec4 aColor;\n"
        "layout(location = 2) in float aSize;\n"
        "uniform mat4 uViewProj;\n"
        "out vec4 vColor;\n"
        "void main() {\n"
        "    gl_Position = uViewProj * vec4(aPosition, 1.0);\n"
        "    float dist = max(gl_Position.w, 0.1);\n"
        "    gl_PointSize = clamp(aSize * 450.0 / dist, 2.0, 64.0);\n"
        "    vColor = aColor;\n"
        "}\n";

    const char* frag_src_gl3 =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec4 vColor;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;\n"
        "    float distSq = dot(circCoord, circCoord);\n"
        "    if (distSq > 1.0) discard;\n"
        "    float intensity = 1.0 - distSq;\n"
        "    fragColor = vec4(vColor.rgb * (1.0 + intensity * 0.6), vColor.a * intensity);\n"
        "}\n";

    const char* vert_src_gl2 =
        "#version 100\n"
        "attribute vec3 aPosition;\n"
        "attribute vec4 aColor;\n"
        "attribute float aSize;\n"
        "uniform mat4 uViewProj;\n"
        "varying vec4 vColor;\n"
        "void main() {\n"
        "    gl_Position = uViewProj * vec4(aPosition, 1.0);\n"
        "    float dist = max(gl_Position.w, 0.1);\n"
        "    gl_PointSize = clamp(aSize * 450.0 / dist, 2.0, 64.0);\n"
        "    vColor = aColor;\n"
        "}\n";

    const char* frag_src_gl2 =
        "#version 100\n"
        "precision mediump float;\n"
        "varying vec4 vColor;\n"
        "void main() {\n"
        "    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;\n"
        "    float distSq = dot(circCoord, circCoord);\n"
        "    if (distSq > 1.0) discard;\n"
        "    float intensity = 1.0 - distSq;\n"
        "    gl_FragColor = vec4(vColor.rgb * (1.0 + intensity * 0.6), vColor.a * intensity);\n"
        "}\n";

    shader_program_ = createProgram(vert_src_gl3, frag_src_gl3);
    if (!shader_program_) {
        LOGI("ParticleSystem falling back to GL2/ES2 shaders");
        shader_program_ = createProgram(vert_src_gl2, frag_src_gl2);
    }
    if (!shader_program_) {
        LOGE("Failed to compile ParticleSystem shader program!");
        return;
    }

    u_view_proj_loc_ = glGetUniformLocation(shader_program_, "uViewProj");

#if !defined(__ANDROID__)
    if (glGenVertexArrays) {
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
    }
#endif

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, max_capacity_ * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

    // Attribute 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
                          reinterpret_cast<void*>(offsetof(ParticleVertex, position)));

    // Attribute 1: color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
                          reinterpret_cast<void*>(offsetof(ParticleVertex, color)));

    // Attribute 2: size
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
                          reinterpret_cast<void*>(offsetof(ParticleVertex, size)));

    if (vao_ != 0) {
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    gl_initialized_ = true;
    LOGI("ParticleSystem GL initialized successfully (max_particles=%zu)", max_capacity_);
}

void ParticleSystem::cleanupGL() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (shader_program_ != 0) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
    gl_initialized_ = false;
}

void ParticleSystem::render(const glm::mat4& view_proj) {
    if (active_count_ == 0) return;

    if (!gl_initialized_) {
        initGL();
        if (!gl_initialized_) return;
    }

    for (size_t i = 0; i < active_count_; ++i) {
        vertex_buffer_[i].position = particles_[i].position;
        vertex_buffer_[i].color = particles_[i].color;
        vertex_buffer_[i].size = particles_[i].size;
    }

#if defined(GL_PROGRAM_POINT_SIZE)
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for sparks
    glDepthMask(GL_FALSE);

    glUseProgram(shader_program_);
    if (u_view_proj_loc_ >= 0) {
        glUniformMatrix4fv(u_view_proj_loc_, 1, GL_FALSE, glm::value_ptr(view_proj));
    }

    if (vao_ != 0) {
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, active_count_ * sizeof(ParticleVertex), vertex_buffer_.data());
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(active_count_));
        glBindVertexArray(0);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, active_count_ * sizeof(ParticleVertex), vertex_buffer_.data());

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
                              reinterpret_cast<void*>(offsetof(ParticleVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
                              reinterpret_cast<void*>(offsetof(ParticleVertex, color)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
                              reinterpret_cast<void*>(offsetof(ParticleVertex, size)));

        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(active_count_));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}
