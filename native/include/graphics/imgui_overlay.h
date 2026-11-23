// Copyright (C) 2017-2026 Chris Liebert

#ifndef _IMGUI_OVERLAY_H_
#define _IMGUI_OVERLAY_H_

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct GLFWwindow;

struct TelemetryData {
    int fps = 0;
    float frame_time_ms = 0.0f;
    int rigid_body_count = 0;
    int culled_count = 0;
    int rendered_count = 0;
    int draw_calls = 0;
};

struct SimulationControls {
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float restitution = 0.6f;
    float friction = 0.5f;
    float time_scale = 1.0f; // 0.1x to 2.0x
    bool paused = false;
    bool step_requested = false;
    glm::vec3 light_pos = glm::vec3(20.0f, 40.0f, 20.0f);
    float ambient_intensity = 0.25f;
    float camera_fov = 45.0f;
    float camera_speed = 1.0f;
    bool debug_draw_wireframe = false;
    bool debug_draw_aabb = false;
    bool debug_draw_contact_points = false;
    int selected_preset = 0; // 0: Default, 1: Jenga, 2: Wrecking Ball, 3: Compound, 4: Stress Benchmark
    bool preset_changed = false;
    bool hot_reload_requested = false;
};

class ImGuiOverlay {
private:
    bool initialized = false;
    GLFWwindow* window_handle = nullptr;
    SimulationControls controls;
    TelemetryData telemetry;

    static constexpr size_t FRAME_HISTORY_SIZE = 120;
    float frame_time_history[FRAME_HISTORY_SIZE] = {0.0f};
    size_t history_index = 0;
    size_t history_count = 0;
    float min_frame_time = 0.0f;
    float max_frame_time = 0.0f;
    float avg_frame_time = 0.0f;

public:
    ImGuiOverlay();
    ~ImGuiOverlay();

    // Lifecycle
    bool init(GLFWwindow* window);
    void shutdown();

    // Frame orchestration
    void beginFrame();
    void renderHUD(const TelemetryData& data);
    void renderHUD(int fps, float frame_time_ms, int rigid_body_count, int culled_count, int rendered_count, int draw_calls);
    void endFrame();

    // Input gating
    bool wantCaptureMouse() const;
    bool wantCaptureKeyboard() const;

    // Controls and Telemetry access
    SimulationControls& getControls() { return controls; }
    const SimulationControls& getControls() const { return controls; }
    const TelemetryData& getTelemetry() const { return telemetry; }

    void setGravity(const glm::vec3& g) { controls.gravity = g; }
    glm::vec3 getGravity() const { return controls.gravity; }

    void setTimeScale(float scale) { controls.time_scale = scale; }
    float getTimeScale() const { return controls.time_scale; }

    bool isPaused() const { return controls.paused; }
    void setPaused(bool p) { controls.paused = p; }

    bool consumeStepRequest() {
        bool req = controls.step_requested;
        controls.step_requested = false;
        return req;
    }

    bool consumePresetChange(int& out_preset) {
        if (controls.preset_changed) {
            controls.preset_changed = false;
            out_preset = controls.selected_preset;
            return true;
        }
        return false;
    }

    bool consumeHotReloadRequest() {
        bool req = controls.hot_reload_requested;
        controls.hot_reload_requested = false;
        return req;
    }

    bool isInitialized() const { return initialized; }
};

#endif // _IMGUI_OVERLAY_H_
