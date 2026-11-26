// Copyright (C) 2017-2026 Chris Liebert
// Unit tests for ShadowMap light space matrices, orthographic bounds, and PCF depth bias

#include "test_helpers.h"
#include "graphics/shadow_map.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <vector>

// Analytical slope-scaled depth bias helper matching standard.frag
inline float calculateSlopeScaledBias(const glm::vec3& normal, const glm::vec3& lightDir) {
    float n_dot_l = glm::dot(glm::normalize(normal), glm::normalize(lightDir));
    return std::max(0.005f * (1.0f - n_dot_l), 0.0005f);
}

int runShadowMapTests() {
    int failures = 0;

    const glm::vec3 light_pos(20.0f, 40.0f, 20.0f);
    const glm::vec3 target_pos(0.0f, 0.0f, 0.0f);
    const glm::vec3 up_vec(0.0f, 1.0f, 0.0f);
    const float ortho_size = 30.0f;
    const float near_plane = 1.0f;
    const float far_plane = 100.0f;

    // =========================================================================
    // Test 1: Light View Matrix Generation (glm::lookAt)
    // =========================================================================
    {
        glm::mat4 light_view = glm::lookAt(light_pos, target_pos, up_vec);

        // Light position must transform to eye space origin (0, 0, 0)
        glm::vec4 eye_light = light_view * glm::vec4(light_pos, 1.0f);
        ASSERT_NEAR(eye_light.x, 0.0f, 1e-4f);
        ASSERT_NEAR(eye_light.y, 0.0f, 1e-4f);
        ASSERT_NEAR(eye_light.z, 0.0f, 1e-4f);
        ASSERT_NEAR(eye_light.w, 1.0f, 1e-4f);

        // Target position must transform to (0, 0, -distance) in OpenGL eye space
        float expected_distance = glm::length(light_pos - target_pos);
        glm::vec4 eye_target = light_view * glm::vec4(target_pos, 1.0f);
        ASSERT_NEAR(eye_target.x, 0.0f, 1e-4f);
        ASSERT_NEAR(eye_target.y, 0.0f, 1e-4f);
        ASSERT_NEAR(eye_target.z, -expected_distance, 1e-3f);

        // View matrix upper 3x3 must be strictly orthonormal
        glm::vec3 r(light_view[0][0], light_view[1][0], light_view[2][0]);
        glm::vec3 u(light_view[0][1], light_view[1][1], light_view[2][1]);
        glm::vec3 f(light_view[0][2], light_view[1][2], light_view[2][2]);

        ASSERT_NEAR(glm::length(r), 1.0f, 1e-5f);
        ASSERT_NEAR(glm::length(u), 1.0f, 1e-5f);
        ASSERT_NEAR(glm::length(f), 1.0f, 1e-5f);
        ASSERT_NEAR(glm::dot(r, u), 0.0f, 1e-5f);
        ASSERT_NEAR(glm::dot(r, f), 0.0f, 1e-5f);
        ASSERT_NEAR(glm::dot(u, f), 0.0f, 1e-5f);
    }

    // =========================================================================
    // Test 2: Light Orthographic Projection Bounds
    // =========================================================================
    {
        glm::mat4 light_proj = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);

        // Diagonal scale factors for [-30, 30] box
        ASSERT_NEAR(light_proj[0][0], 1.0f / ortho_size, 1e-5f);
        ASSERT_NEAR(light_proj[1][1], 1.0f / ortho_size, 1e-5f);
        ASSERT_NEAR(light_proj[2][2], -2.0f / (far_plane - near_plane), 1e-5f);
        ASSERT_NEAR(light_proj[3][3], 1.0f, 1e-5f);

        // Depth translation component
        float expected_tz = -(far_plane + near_plane) / (far_plane - near_plane);
        ASSERT_NEAR(light_proj[3][2], expected_tz, 1e-4f);
    }

    // =========================================================================
    // Test 3: World Points Transformation into Light Clip Space [-1, 1]
    // =========================================================================
    {
        glm::mat4 lsm = ShadowMap::computeLightSpaceMatrix(
            light_pos, target_pos, up_vec, ortho_size, near_plane, far_plane);

        // Target point (0, 0, 0) should be at center of light viewport (x~0, y~0, z inside [-1, 1])
        glm::vec4 clip_target = lsm * glm::vec4(target_pos, 1.0f);
        glm::vec3 ndc_target = glm::vec3(clip_target) / clip_target.w;
        ASSERT_NEAR(ndc_target.x, 0.0f, 1e-4f);
        ASSERT_NEAR(ndc_target.y, 0.0f, 1e-4f);
        ASSERT_TRUE(ndc_target.z >= -1.0f && ndc_target.z <= 1.0f);

        // Compute camera coordinate vectors in world space
        glm::vec3 forward_dir = glm::normalize(target_pos - light_pos);
        glm::vec3 right_dir = glm::normalize(glm::cross(forward_dir, up_vec));
        glm::vec3 true_up_dir = glm::cross(right_dir, forward_dir);

        // Near plane center point: light_pos + forward * near_plane -> clip z = -1.0
        glm::vec3 near_center = light_pos + forward_dir * near_plane;
        glm::vec4 clip_near = lsm * glm::vec4(near_center, 1.0f);
        glm::vec3 ndc_near = glm::vec3(clip_near) / clip_near.w;
        ASSERT_NEAR(ndc_near.z, -1.0f, 1e-4f);

        // Far plane center point: light_pos + forward * far_plane -> clip z = +1.0
        glm::vec3 far_center = light_pos + forward_dir * far_plane;
        glm::vec4 clip_far = lsm * glm::vec4(far_center, 1.0f);
        glm::vec3 ndc_far = glm::vec3(clip_far) / clip_far.w;
        ASSERT_NEAR(ndc_far.z, 1.0f, 1e-4f);

        // Lateral ortho bounds (+/- ortho_size along right and up axes)
        glm::vec3 right_bound = target_pos + right_dir * ortho_size;
        glm::vec4 clip_rb = lsm * glm::vec4(right_bound, 1.0f);
        glm::vec3 ndc_rb = glm::vec3(clip_rb) / clip_rb.w;
        ASSERT_NEAR(ndc_rb.x, 1.0f, 1e-3f);

        glm::vec3 left_bound = target_pos - right_dir * ortho_size;
        glm::vec4 clip_lb = lsm * glm::vec4(left_bound, 1.0f);
        glm::vec3 ndc_lb = glm::vec3(clip_lb) / clip_lb.w;
        ASSERT_NEAR(ndc_lb.x, -1.0f, 1e-3f);

        glm::vec3 top_bound = target_pos + true_up_dir * ortho_size;
        glm::vec4 clip_tb = lsm * glm::vec4(top_bound, 1.0f);
        glm::vec3 ndc_tb = glm::vec3(clip_tb) / clip_tb.w;
        ASSERT_NEAR(ndc_tb.y, 1.0f, 1e-3f);

        glm::vec3 bottom_bound = target_pos - true_up_dir * ortho_size;
        glm::vec4 clip_bb = lsm * glm::vec4(bottom_bound, 1.0f);
        glm::vec3 ndc_bb = glm::vec3(clip_bb) / clip_bb.w;
        ASSERT_NEAR(ndc_bb.y, -1.0f, 1e-3f);
    }

    // =========================================================================
    // Test 4: Slope-Scaled Depth Bias Calculation
    // =========================================================================
    {
        // Case A: Parallel surface (n dot l == 1.0)
        // Bias formula: max(0.005 * (1.0 - 1.0), 0.0005) == 0.0005 (minimum floor)
        glm::vec3 normal_parallel(0.0f, 1.0f, 0.0f);
        glm::vec3 light_parallel(0.0f, 1.0f, 0.0f);
        float bias_parallel = calculateSlopeScaledBias(normal_parallel, light_parallel);
        ASSERT_NEAR(bias_parallel, 0.0005f, 1e-6f);

        // Case B: Perpendicular surface (n dot l == 0.0)
        // Bias formula: max(0.005 * (1.0 - 0.0), 0.0005) == 0.005
        glm::vec3 normal_perp(1.0f, 0.0f, 0.0f);
        glm::vec3 light_perp(0.0f, 1.0f, 0.0f);
        float bias_perp = calculateSlopeScaledBias(normal_perp, light_perp);
        ASSERT_NEAR(bias_perp, 0.005f, 1e-6f);

        // Case C: Oblique surface at 45 degrees (n dot l == cos(45 deg) = sqrt(2)/2)
        // Bias formula: max(0.005 * (1.0 - 0.70710678), 0.0005) = 0.005 * 0.2928932 = 0.00146447
        glm::vec3 normal_45(0.0f, 1.0f, 0.0f);
        glm::vec3 light_45 = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
        float bias_45 = calculateSlopeScaledBias(normal_45, light_45);
        ASSERT_NEAR(bias_45, 0.00146447f, 1e-5f);

        // Case D: Oblique surface at 60 degrees (n dot l == cos(60 deg) = 0.5)
        // Bias formula: max(0.005 * (1.0 - 0.5), 0.0005) = 0.0025
        glm::vec3 normal_60(0.0f, 1.0f, 0.0f);
        glm::vec3 light_60 = glm::normalize(glm::vec3(std::sqrt(3.0f), 1.0f, 0.0f));
        float bias_60 = calculateSlopeScaledBias(normal_60, light_60);
        ASSERT_NEAR(bias_60, 0.0025f, 1e-5f);

        // Case E: Clamping floor test when n dot l is 0.95 (0.005 * 0.05 = 0.00025 < 0.0005)
        glm::vec3 normal_near_parallel(0.0f, 1.0f, 0.0f);
        glm::vec3 light_near_parallel = glm::normalize(glm::vec3(0.1f, 1.0f, 0.0f));
        float bias_clamped = calculateSlopeScaledBias(normal_near_parallel, light_near_parallel);
        ASSERT_NEAR(bias_clamped, 0.0005f, 1e-6f);
    }

    // =========================================================================
    // Test 5: 3x3 PCF Kernel Tap Offset Coordinates
    // =========================================================================
    {
        const int map_width = 2048;
        const int map_height = 2048;
        const glm::vec2 texel_size(1.0f / map_width, 1.0f / map_height);

        std::vector<glm::vec2> tap_offsets;
        float total_weight = 0.0f;
        const float tap_weight = 1.0f / 9.0f;

        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                glm::vec2 offset(static_cast<float>(x) * texel_size.x,
                                 static_cast<float>(y) * texel_size.y);
                tap_offsets.push_back(offset);
                total_weight += tap_weight;
            }
        }

        // Must generate exactly 9 taps
        ASSERT_TRUE(tap_offsets.size() == 9);

        // Sum of all weights must be 1.0
        ASSERT_NEAR(total_weight, 1.0f, 1e-6f);

        // Center tap (x=0, y=0) is offset (0, 0)
        ASSERT_NEAR(tap_offsets[4].x, 0.0f, 1e-7f);
        ASSERT_NEAR(tap_offsets[4].y, 0.0f, 1e-7f);

        // Corner tap (-1, -1)
        ASSERT_NEAR(tap_offsets[0].x, -texel_size.x, 1e-7f);
        ASSERT_NEAR(tap_offsets[0].y, -texel_size.y, 1e-7f);

        // Corner tap (+1, +1)
        ASSERT_NEAR(tap_offsets[8].x, texel_size.x, 1e-7f);
        ASSERT_NEAR(tap_offsets[8].y, texel_size.y, 1e-7f);
    }

    // =========================================================================
    // Test 6: ShadowMap Class State & Matrix Updates
    // =========================================================================
    {
        ShadowMap sm;
        ASSERT_TRUE(sm.getShadowWidth() == 2048);
        ASSERT_TRUE(sm.getShadowHeight() == 2048);
        ASSERT_FALSE(sm.isInitialized());
        ASSERT_VEC3_NEAR(sm.getLightPos(), glm::vec3(20.0f, 40.0f, 20.0f), 1e-4f);

        // Update light position
        glm::vec3 new_light_pos(10.0f, 25.0f, -15.0f);
        sm.updateLightSpaceMatrix(new_light_pos, target_pos, up_vec, 25.0f, 2.0f, 80.0f);

        ASSERT_VEC3_NEAR(sm.getLightPos(), new_light_pos, 1e-4f);

        glm::mat4 expected_matrix = ShadowMap::computeLightSpaceMatrix(
            new_light_pos, target_pos, up_vec, 25.0f, 2.0f, 80.0f);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                ASSERT_NEAR(sm.getLightSpaceMatrix()[i][j], expected_matrix[i][j], 1e-4f);
            }
        }
    }

    return failures;
}
