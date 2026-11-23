// Copyright (C) 2017-2026 Chris Liebert

#ifndef _DROIDGFXSIM_PARTICLE_SYSTEM_H_
#define _DROIDGFXSIM_PARTICLE_SYSTEM_H_

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <cstddef>

struct Particle {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec4 color{1.0f, 0.6f, 0.2f, 1.0f};
    float life{0.0f};
    float max_life{1.0f};
    float size{0.1f};
    float gravity{-12.0f};
    float drag{0.98f};
    float size_growth{0.0f};
};

struct ParticleVertex {
    glm::vec3 position;
    glm::vec4 color;
    float size;
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    void init(size_t max_particles = 5000);
    void emitBurst(const glm::vec3& position, const glm::vec3& normal, int count = 40,
                   const glm::vec4& color = glm::vec4(1.0f, 0.6f, 0.2f, 1.0f));
    void emitDebrisBurst(const glm::vec3& position, const glm::vec3& normal, int count = 40,
                         int burst_type = 0);
    void update(float dt);
    void render(const glm::mat4& view_proj);
    [[nodiscard]] size_t getActiveCount() const;
    [[nodiscard]] size_t getMaxCapacity() const { return max_capacity_; }
    void clear();

private:
    void initGL();
    void cleanupGL();

    std::vector<Particle> particles_;
    std::vector<ParticleVertex> vertex_buffer_;
    size_t max_capacity_{5000};
    size_t active_count_{0};

    unsigned int vao_{0};
    unsigned int vbo_{0};
    unsigned int shader_program_{0};
    int u_view_proj_loc_{-1};
    bool gl_initialized_{false};
};

#endif // _DROIDGFXSIM_PARTICLE_SYSTEM_H_
