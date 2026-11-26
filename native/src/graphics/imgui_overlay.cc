// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/imgui_overlay.h"
#include "common/log.h"
#include <algorithm>
#include <cstdio>

#if !defined(__ANDROID__)
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#if defined(DROIDGFXSIM_IMGUI_HAS_VULKAN)
#include "imgui_impl_vulkan.h"
#include <vulkan/vulkan.h>
#endif
#include <GLFW/glfw3.h>
#endif

ImGuiOverlay::ImGuiOverlay()
    : initialized(false),
      vulkan_backend(false),
      window_handle(nullptr),
      vk_device(nullptr),
      vk_descriptor_pool(nullptr),
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

void ImGuiOverlay::configureScaledFonts() {
#if !defined(__ANDROID__)
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 26.0f; // Crisp 2x scale over 13px ProggyClean
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    io.Fonts->AddFontDefault(&font_cfg);
    io.FontGlobalScale = 1.0f;
#endif
}

void ImGuiOverlay::applyGlassTheme() {
#if !defined(__ANDROID__)
    // Reset style before scaling so repeated initialization never compounds ScaleAllSizes
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);

    // Portfolio aesthetic styling (high-contrast translucent dark-glass HUD)
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.5f;

    style.Colors[ImGuiCol_Text] = ImVec4(0.96f, 0.98f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.11f, 0.36f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.07f, 0.11f, 0.20f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.19f, 0.27f, 0.44f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.30f, 0.42f, 0.58f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.38f, 0.54f, 0.68f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.08f, 0.12f, 0.54f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.18f, 0.30f, 0.66f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.07f, 0.11f, 0.34f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.25f, 0.38f, 0.48f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.36f, 0.52f, 0.62f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.42f, 0.60f, 0.72f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.28f, 0.42f, 0.54f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.38f, 0.56f, 0.70f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.34f, 0.50f, 0.78f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.09f, 0.13f, 0.84f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.68f, 0.78f, 0.92f, 0.42f);

    style.ScaleAllSizes(2.0f);
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.ItemSpacing = ImVec2(10.0f, 7.0f);
#endif
}

bool ImGuiOverlay::init(GLFWwindow* window) {
#if !defined(__ANDROID__)
    if (initialized) return true;
    if (!window) {
        LOGE("ImGuiOverlay::init received null GLFWwindow pointer!");
        return false;
    }

    window_handle = window;
    vulkan_backend = false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    configureScaledFonts();
    applyGlassTheme();

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

bool ImGuiOverlay::initVulkan(GLFWwindow* window, const ImGuiVulkanInitInfo& info) {
#if !defined(__ANDROID__) && defined(DROIDGFXSIM_IMGUI_HAS_VULKAN)
    if (initialized) return true;
    if (!window || !info.instance || !info.physical_device || !info.device || !info.queue || !info.render_pass) {
        LOGE("ImGuiOverlay::initVulkan received invalid window or Vulkan handles");
        return false;
    }

    VkDevice vkDev = static_cast<VkDevice>(info.device);
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 }
    };
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 64;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool desc_pool = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorPool(vkDev, &pool_info, nullptr, &desc_pool);
    if (res != VK_SUCCESS || desc_pool == VK_NULL_HANDLE) {
        LOGE("ImGuiOverlay::initVulkan failed to create VkDescriptorPool (VkResult %d)", res);
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    configureScaledFonts();
    applyGlassTheme();

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        LOGE("ImGuiOverlay::initVulkan failed ImGui_ImplGlfw_InitForVulkan");
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(vkDev, desc_pool, nullptr);
        return false;
    }

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = static_cast<VkInstance>(info.instance);
    init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(info.physical_device);
    init_info.Device = vkDev;
    init_info.QueueFamily = info.queue_family;
    init_info.Queue = static_cast<VkQueue>(info.queue);
    init_info.DescriptorPool = desc_pool;
    init_info.RenderPass = static_cast<VkRenderPass>(info.render_pass);
    init_info.MinImageCount = std::max(2u, info.min_image_count);
    init_info.ImageCount = std::max(init_info.MinImageCount, info.image_count);
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        LOGE("ImGuiOverlay::initVulkan failed ImGui_ImplVulkan_Init");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(vkDev, desc_pool, nullptr);
        return false;
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        LOGE("ImGuiOverlay::initVulkan failed ImGui_ImplVulkan_CreateFontsTexture");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(vkDev, desc_pool, nullptr);
        return false;
    }

    window_handle = window;
    vulkan_backend = true;
    vk_device = info.device;
    vk_descriptor_pool = static_cast<void*>(desc_pool);
    initialized = true;
    LOGI("ImGuiOverlay initialized successfully with GLFW and Vulkan backends");
    return true;
#else
    (void)window;
    (void)info;
    return false;
#endif
}

void ImGuiOverlay::shutdown() {
#if !defined(__ANDROID__)
    if (!initialized) return;

#if defined(DROIDGFXSIM_IMGUI_HAS_VULKAN)
    if (vulkan_backend) {
        VkDevice vkDev = static_cast<VkDevice>(vk_device);
        if (vkDev != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(vkDev);
        }
        ImGui_ImplVulkan_Shutdown();
        if (vkDev != VK_NULL_HANDLE && vk_descriptor_pool != nullptr) {
            vkDestroyDescriptorPool(vkDev, static_cast<VkDescriptorPool>(vk_descriptor_pool), nullptr);
        }
        vk_descriptor_pool = nullptr;
        vk_device = nullptr;
    } else {
        ImGui_ImplOpenGL3_Shutdown();
    }
#else
    ImGui_ImplOpenGL3_Shutdown();
#endif

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    initialized = false;
    vulkan_backend = false;
    window_handle = nullptr;
    LOGI("ImGuiOverlay shutdown completed");
#endif
}

void ImGuiOverlay::beginFrame() {
#if !defined(__ANDROID__)
    if (!initialized) return;

#if defined(DROIDGFXSIM_IMGUI_HAS_VULKAN)
    if (vulkan_backend) {
        ImGui_ImplVulkan_NewFrame();
    } else {
        ImGui_ImplOpenGL3_NewFrame();
    }
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
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

    // HUD window configuration: sleek compact 2x-DPI translucent overlay anchored top-left
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(495.0f, 0.0f), ImVec2(550.0f, 760.0f));
    ImGui::SetNextWindowBgAlpha(0.36f);

    if (ImGui::Begin("droidgfxsim Telemetry & Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushItemWidth(210.0f);

        // Section 1: Telemetry Dashboard
        if (ImGui::CollapsingHeader("Telemetry & Profiling", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "FPS: %d (%.2f ms)", telemetry.fps, telemetry.frame_time_ms);

            char overlay[64];
            std::snprintf(overlay, sizeof(overlay), "avg %.1fms (%.1f - %.1f)", avg_frame_time, min_frame_time, max_frame_time);
            ImGui::PlotLines("##FrameTime", frame_time_history, static_cast<int>(FRAME_HISTORY_SIZE),
                             static_cast<int>(history_index), overlay, 0.0f, 40.0f, ImVec2(-1.0f, 70.0f));

            ImGui::Separator();
            ImGui::Text("Bodies: %d | Drawn: %d", telemetry.rigid_body_count, telemetry.rendered_count);
            ImGui::Text("Culled: %d | Calls: %d", telemetry.culled_count, telemetry.draw_calls);
        }

        // Section 2: Physics Simulation Controls (collapsed by default for compact 2x layout)
        if (ImGui::CollapsingHeader("Physics Simulation")) {
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

        // Section 3: Lighting & Camera Controls (collapsed by default)
        if (ImGui::CollapsingHeader("Lighting & Camera")) {
            float lp[3] = {controls.light_pos.x, controls.light_pos.y, controls.light_pos.z};
            if (ImGui::SliderFloat3("Light Position", lp, -50.0f, 50.0f, "%.1f")) {
                controls.light_pos = glm::vec3(lp[0], lp[1], lp[2]);
            }

            ImGui::SliderFloat("Ambient Intensity", &controls.ambient_intensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Camera FOV", &controls.camera_fov, 20.0f, 100.0f, "%.0f°");
            ImGui::SliderFloat("Camera Speed", &controls.camera_speed, 0.2f, 5.0f, "%.1fx");
        }

        // Section 4: Bullet Debug Visualizer Toggles (collapsed by default)
        if (ImGui::CollapsingHeader("Bullet Debug Visualizer")) {
            ImGui::Checkbox("Draw Wireframe Collision Hulls (F1)", &controls.debug_draw_wireframe);
            ImGui::Checkbox("Draw Bounding Boxes (AABB)", &controls.debug_draw_aabb);
            ImGui::Checkbox("Draw Contact Manifold Points", &controls.debug_draw_contact_points);
        }

        // Section 5: Shader Hot-Reloading
        if (ImGui::CollapsingHeader("Shader Pipeline")) {
            if (ImGui::Button("Atomic Hot-Reload Shaders (R)")) {
                controls.hot_reload_requested = true;
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Compiles candidates and swaps atomically without crashes.");
            ImGui::PopTextWrapPos();
        }

        ImGui::PopItemWidth();
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
    if (!vulkan_backend) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
#endif
}

void ImGuiOverlay::renderVulkanDrawData(void* command_buffer) {
#if !defined(__ANDROID__) && defined(DROIDGFXSIM_IMGUI_HAS_VULKAN)
    if (!initialized || !vulkan_backend || command_buffer == nullptr) return;
    ImDrawData* dd = ImGui::GetDrawData();
    if (dd && dd->Valid && dd->CmdListsCount > 0) {
        ImGui_ImplVulkan_RenderDrawData(dd, static_cast<VkCommandBuffer>(command_buffer));
    }
#else
    (void)command_buffer;
#endif
}

void ImGuiOverlay::setVulkanMinImageCount(uint32_t min_image_count) {
#if !defined(__ANDROID__) && defined(DROIDGFXSIM_IMGUI_HAS_VULKAN)
    if (initialized && vulkan_backend) {
        ImGui_ImplVulkan_SetMinImageCount(std::max(2u, min_image_count));
    }
#else
    (void)min_image_count;
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
