#include <iostream>

int runMathTests();
int runCameraTests();
int runFrustumTests();
int runSceneNodeTests();
int runAssetManagerTests();
int runWavefrontTests();
int runPhysicsTests();
int runRaycastTests();
int runShadowMapTests();
int runShaderManagerTests();
#if defined(DROIDGFXSIM_HAS_VULKAN) || defined(Vulkan_FOUND)
int runVulkanTests();
#endif

struct TestSuite {
    const char* name;
    int (*func)();
};

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  droidgfxsim Modernized Unit Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    TestSuite suites[] = {
        {"Math & Welzl Bounding Sphere", runMathTests},
        {"Camera Orientation & Clamping", runCameraTests},
        {"Frustum Culling & Planes", runFrustumTests},
        {"Scene Graph Hierarchy & Lifecycle", runSceneNodeTests},
        {"Asset Manager & Path Resolution", runAssetManagerTests},
        {"Wavefront Parser & Material Binding", runWavefrontTests},
        {"Bullet Physics Simulation & Cleanup", runPhysicsTests},
        {"Raycast Picker & Dynamic Constraints", runRaycastTests},
        {"Shadow Map Matrices & PCF Filtering", runShadowMapTests},
        {"Shader Manager & Atomic Hot-Reload", runShaderManagerTests},
#if defined(DROIDGFXSIM_HAS_VULKAN) || defined(Vulkan_FOUND)
        {"Vulkan Environment & Headless Initialization", runVulkanTests},
#endif
    };

    int passed = 0;
    int failed = 0;

    for (const auto& s : suites) {
        std::cout << "\n--- Running: " << s.name << " ---" << std::endl;
        int result = s.func();
        if (result == 0) {
            std::cout << "[PASS] " << s.name << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] " << s.name << " (" << result << " failures)" << std::endl;
            failed++;
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Summary: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
