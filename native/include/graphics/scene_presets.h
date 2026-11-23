// Copyright (C) 2017-2026 Chris Liebert
// Multi-Scene Preset Selector & Dynamic Physics Sandboxes

#ifndef _DROIDGFXSIM_SCENE_PRESETS_H_
#define _DROIDGFXSIM_SCENE_PRESETS_H_

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

class Application;
class Simulation;
class Camera;

namespace droidgfxsim {

/// @brief Identifiers for the 4 canonical physics presets.
enum class PresetIndex {
    Jenga = 0,
    WreckingBall = 1,
    Compound = 2,
    StressTest = 3,
    Count = 4
};

/// @brief Metadata descriptor for a scene preset sandbox.
struct PresetMetadata {
    int id{0};
    const char* name{""};
    const char* description{""};
    const char* xml_file{""};
    glm::vec3 camera_pos{0.0f, 10.0f, 25.0f};
    double camera_yaw{0.0};
    double camera_pitch{0.0};
};

/// @brief Manages loading and switching between distinct dynamic physics sandbox environments without memory leaks.
class ScenePresets {
public:
    static constexpr int kPresetCount = 4;

    /// @brief Retrieve metadata for the preset at index.
    static const PresetMetadata& getPresetInfo(int index);

    /// @brief Get the user-facing name for preset index.
    static const char* getPresetName(int index);

    /// @brief Get the asset XML file path for preset index.
    static const char* getPresetXmlPath(int index);

    /// @brief Total number of available presets.
    static constexpr int getPresetCount() noexcept { return kPresetCount; }

    /// @brief Load and activate a scene preset into the running application or simulation.
    /// Resets existing scenegraph and simulation cleanly, loads preset XML, adjusts camera
    /// to optimal viewpoint, and configures preset-specific constraints.
    /// @param preset_index Zero-based preset index (0: Jenga, 1: WreckingBall, 2: Compound, 3: StressTest).
    /// @param app Pointer to Application instance (may be nullptr in isolated test harness).
    /// @param sim Pointer to Simulation instance (if nullptr and app != nullptr, uses app->simulation).
    /// @param camera Pointer to Camera instance (if nullptr and app != nullptr, uses app->camera).
    /// @return True on successful load.
    static bool loadPreset(int preset_index, Application* app, Simulation* sim = nullptr,
                           Camera* camera = nullptr);

    /// @brief Overload accepting string preset name (case-insensitive substring match).
    static bool loadPreset(const std::string& name, Application* app, Simulation* sim = nullptr,
                           Camera* camera = nullptr);

    /// @brief Configure camera viewpoint and angles for a preset.
    static void configureCamera(int preset_index, Camera* camera);
};

// Alias for architecture consistency
using ScenePresetManager = ScenePresets;

} // namespace droidgfxsim

#endif // _DROIDGFXSIM_SCENE_PRESETS_H_
