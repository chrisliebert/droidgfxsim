// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/imgui_overlay.h"
#include "common/log.h"
#include <algorithm>
#include <cstdio>

#if !defined(__ANDROID__)
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#endif

ImGuiOverlay::ImGuiOverlay()
    : initialized(false),
      window_handle(nullptr),
      history_index(0),
      history_count(0),
      min_frame_time(0.0f),
      max_frame_time(0.0f),
      avg_frame_time(0.0f) {
    for (size_t i = 0; i < FRAME_HISTORY_SIZE; ++i) {
        frame_time_history[i] = 0.0f;
    }
}

ImGuiOverlay::~ImGuiOverlay() {
    shutdown();
}

bool ImGuiOverlay::init(GLFWwindow* window) {
#if !defined(__ANDROID__)
    if (initialized) return true;
    if (!window) {
        LOGE("ImGuiOverlay::init received null GLFWwindow pointer!");
        return false;
    }

    window_handle = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Portfolio aesthetic styling
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.5f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);

#if defined(__EMSCRIPTEN__)
    ImGui_ImplOpenGL3_Init("#version 300 es");
#elif defined(__APPLE__) && defined(TARGET_OS_IPHONE)
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 330 core");
#endif

    initialized = true;
    LOGI("ImGuiOverlay initialized successfully with GLFW and OpenGL3 backends");
    return true;
#else
    (void)window;
    return false;
#endif
}

void ImGuiOverlay::shutdown() {
#if !defined(__ANDROID__)
    if (!initialized) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    initialized = false;
    window_handle = nullptr;
    LOGI("ImGuiOverlay shutdown completed");
#endif
}

void ImGuiOverlay::beginFrame() {
#if !defined(__ANDROID__)
    if (!initialized) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiOverlay::renderHUD(int fps, float frame_time_ms, int rigid_body_count, int culled_count, int rendered_count, int draw_calls) {
    TelemetryData data;
    data.fps = fps;
    data.frame_time_ms = frame_time_ms;
    data.rigid_body_count = rigid_body_count;
    data.culled_count = culled_count;
    data.rendered_count = rendered_count;
    data.draw_calls = draw_calls;
    renderHUD(data);
}

void ImGuiOverlay::renderHUD(const TelemetryData& data) {
#if !defined(__ANDROID__)
    if (!initialized) return;

    telemetry = data;

    // Update rolling frame-time buffer
    frame_time_history[history_index] = telemetry.frame_time_ms;
    history_index = (history_index + 1) % FRAME_HISTORY_SIZE;
    if (history_count < FRAME_HISTORY_SIZE) {
        history_count++;
    }

    // Compute min, max, avg
    float sum = 0.0f;
    min_frame_time = frame_time_history[0];
    max_frame_time = frame_time_history[0];
    for (size_t i = 0; i < history_count; ++i) {
        float v = frame_time_history[i];
        sum += v;
        if (v < min_frame_time) min_frame_time = v;
        if (v > max_frame_time) max_frame_time = v;
    }
    avg_frame_time = history_count > 0 ? (sum / static_cast<float>(history_count)) : 0.0f;

    // HUD window configuration
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 540.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("droidgfxsim Telemetry & Controls", nullptr, ImGuiWindowFlags_None)) {
        // Section 1: Telemetry Dashboard
        if (ImGui::CollapsingHeader("Telemetry & Profiling", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "FPS: %d (%.2f ms)", telemetry.fps, telemetry.frame_time_ms);

            char overlay[64];
            std::snprintf(overlay, sizeof(overlay), "avg: %.1fms (min: %.1f, max: %.1f)", avg_frame_time, min_frame_time, max_frame_time);
            ImGui::PlotLines("Frame Time", frame_time_history, static_cast<int>(FRAME_HISTORY_SIZE),
                             static_cast<int>(history_index), overlay, 0.0f, 40.0f, ImVec2(0, 65.0f));

            ImGui::Separator();
            ImGui::Text("Active Rigid Bodies: %d", telemetry.rigid_body_count);
            ImGui::Text("Rendered Scene Nodes: %d", telemetry.rendered_count);
            ImGui::Text("Culled Scene Nodes:   %d", telemetry.culled_count);
            ImGui::Text("Batched Draw Calls:   %d", telemetry.draw_calls);
        }

        // Section 2: Physics Simulation Controls
        if (ImGui::CollapsingHeader("Physics Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
            float g[3] = {controls.gravity.x, controls.gravity.y, controls.gravity.z};
            if (ImGui::SliderFloat3("Gravity (m/s²)", g, -30.0f, 30.0f, "%.1f")) {
                controls.gravity = glm::vec3(g[0], g[1], g[2]);
            }

            ImGui::SliderFloat("Restitution", &controls.restitution, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Friction", &controls.friction, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Time Scale", &controls.time_scale, 0.1f, 2.0f, "%.2fx");

            ImGui::Checkbox("Pause Simulation", &controls.paused);
            ImGui::SameLine();
            if (ImGui::Button("Step Frame")) {
                controls.step_requested = true;
            }

            const char* presets[] = {
                "Default Scene",
                "Jenga Tower",
                "Wrecking Ball Pendulum",
                "Compound Shapes Sandbox",
                "Stress Test Benchmark"
            };
            int current_preset = controls.selected_preset;
            if (ImGui::Combo("Scene Preset", &current_preset, presets, IM_ARRAYSIZE(presets))) {
                if (current_preset != controls.selected_preset) {
                    controls.selected_preset = current_preset;
                    controls.preset_changed = true;
                }
            }
        }

        // Section 3: Lighting & Camera Controls
        if (ImGui::CollapsingHeader("Lighting & Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            float lp[3] = {controls.light_pos.x, controls.light_pos.y, controls.light_pos.z};
            if (ImGui::SliderFloat3("Light Position", lp, -50.0f, 50.0f, "%.1f")) {
                controls.light_pos = glm::vec3(lp[0], lp[1], lp[2]);
            }

            ImGui::SliderFloat("Ambient Intensity", &controls.ambient_intensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Camera FOV", &controls.camera_fov, 20.0f, 100.0f, "%.0f°");
            ImGui::SliderFloat("Camera Speed", &controls.camera_speed, 0.2f, 5.0f, "%.1fx");
        }

        // Section 4: Bullet Debug Visualizer Toggles
        if (ImGui::CollapsingHeader("Bullet Debug Visualizer", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Draw Wireframe Collision Hulls (F1)", &controls.debug_draw_wireframe);
            ImGui::Checkbox("Draw Bounding Boxes (AABB)", &controls.debug_draw_aabb);
            ImGui::Checkbox("Draw Contact Manifold Points", &controls.debug_draw_contact_points);
        }

        // Section 5: Shader Hot-Reloading
        if (ImGui::CollapsingHeader("Shader Pipeline")) {
            if (ImGui::Button("Atomic Hot-Reload Shaders (R)")) {
                controls.hot_reload_requested = true;
            }
            ImGui::TextDisabled("Compiles candidates and swaps atomically without crashes.");
        }
    }
    ImGui::End();
#else
    (void)data;
#endif
}

void ImGuiOverlay::endFrame() {
#if !defined(__ANDROID__)
    if (!initialized) return;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

bool ImGuiOverlay::wantCaptureMouse() const {
#if !defined(__ANDROID__)
    if (initialized && ImGui::GetCurrentContext()) {
        return ImGui::GetIO().WantCaptureMouse;
    }
#endif
    return false;
}

bool ImGuiOverlay::wantCaptureKeyboard() const {
#if !defined(__ANDROID__)
    if (initialized && ImGui::GetCurrentContext()) {
        return ImGui::GetIO().WantCaptureKeyboard;
    }
#endif
    return false;
}
