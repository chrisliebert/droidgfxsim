// Copyright (C) 2017-2026 Chris Liebert
// Unit tests for ShaderManager file loading, version headers, non-crashing compile handling, and fallbacks

#include "test_helpers.h"
#include "graphics/shader_manager.h"
#include "common/asset_manager.hpp"

#include <GLFW/glfw3.h>
#include <string>
#include <vector>

// Helper to test version header injection between GLSL ES 3.0 and Core 3.3
static std::string injectVersionHeader(const std::string& shader_source, bool target_desktop_core) {
    std::string result = shader_source;
    size_t ver_pos = result.find("#version");
    if (ver_pos != std::string::npos) {
        size_t end_line = result.find('\n', ver_pos);
        std::string replacement = target_desktop_core ? "#version 330 core" : "#version 300 es";
        result.replace(ver_pos, end_line - ver_pos, replacement);
    } else {
        std::string header = (target_desktop_core ? "#version 330 core\n" : "#version 300 es\n");
        result = header + result;
    }
    return result;
}

int runShaderManagerTests() {
    int failures = 0;

    AssetManager asset_mgr;

    // =========================================================================
    // Test 1: Shader File Loading via AssetManager
    // =========================================================================
    {
        // 1. standard.vert
        std::string std_vert = asset_mgr.loadTextFile("shaders/standard.vert");
        ASSERT_FALSE(std_vert.empty());
        ASSERT_TRUE(std_vert.find("vPosition") != std::string::npos);
        ASSERT_TRUE(std_vert.find("TransformBlock") != std::string::npos);
        ASSERT_TRUE(std_vert.find("lightSpaceMatrix") != std::string::npos);
        ASSERT_TRUE(std_vert.find("fragPosLightSpace") != std::string::npos);

        // 2. standard.frag
        std::string std_frag = asset_mgr.loadTextFile("shaders/standard.frag");
        ASSERT_FALSE(std_frag.empty());
        ASSERT_TRUE(std_frag.find("diffuseTexture") != std::string::npos);
        ASSERT_TRUE(std_frag.find("shadowMap") != std::string::npos);
        ASSERT_TRUE(std_frag.find("calculateShadow") != std::string::npos);
        ASSERT_TRUE(std_frag.find("texelSize") != std::string::npos);

        // 3. shadow.vert
        std::string shadow_vert = asset_mgr.loadTextFile("shaders/shadow.vert");
        ASSERT_FALSE(shadow_vert.empty());
        ASSERT_TRUE(shadow_vert.find("lightSpaceMatrix") != std::string::npos);
        ASSERT_TRUE(shadow_vert.find("matrix") != std::string::npos);
        ASSERT_TRUE(shadow_vert.find("vPosition") != std::string::npos);

        // 4. shadow.frag
        std::string shadow_frag = asset_mgr.loadTextFile("shaders/shadow.frag");
        ASSERT_FALSE(shadow_frag.empty());
        ASSERT_TRUE(shadow_frag.find("main") != std::string::npos);
    }

    // =========================================================================
    // Test 2: GLSL ES 3.0 vs Core 3.3 Version Header Injection Logic
    // =========================================================================
    {
        std::string gles_src = "#version 300 es\nprecision mediump float;\nvoid main() {}\n";

        // Inject Core 3.3
        std::string core_src = injectVersionHeader(gles_src, true);
        ASSERT_TRUE(core_src.find("#version 330 core") != std::string::npos);
        ASSERT_TRUE(core_src.find("#version 300 es") == std::string::npos);

        // Re-inject ES 3.0
        std::string re_es = injectVersionHeader(core_src, false);
        ASSERT_TRUE(re_es.find("#version 300 es") != std::string::npos);
        ASSERT_TRUE(re_es.find("#version 330 core") == std::string::npos);

        // Header injection on shader with missing header
        std::string bare_src = "void main() { gl_Position = vec4(0.0); }\n";
        std::string injected = injectVersionHeader(bare_src, false);
        ASSERT_TRUE(injected.find("#version 300 es\n") == 0);
    }

    // =========================================================================
    // Test 3: Embedded Fallback Shaders
    // =========================================================================
    {
        const char* fb_std_vert = ShaderManager::getFallbackStandardVertexShader();
        ASSERT_TRUE(fb_std_vert != nullptr);
        std::string str_std_vert(fb_std_vert);
        ASSERT_TRUE(str_std_vert.length() > 50);
        ASSERT_TRUE(str_std_vert.find("TransformBlock") != std::string::npos);
        ASSERT_TRUE(str_std_vert.find("lightSpaceMatrix") != std::string::npos);

        const char* fb_std_frag = ShaderManager::getFallbackStandardFragmentShader();
        ASSERT_TRUE(fb_std_frag != nullptr);
        std::string str_std_frag(fb_std_frag);
        ASSERT_TRUE(str_std_frag.length() > 50);
        ASSERT_TRUE(str_std_frag.find("diffuseTexture") != std::string::npos);
        ASSERT_TRUE(str_std_frag.find("shadowMap") != std::string::npos);
        ASSERT_TRUE(str_std_frag.find("calculateShadow") != std::string::npos);

        const char* fb_shw_vert = ShaderManager::getFallbackShadowVertexShader();
        ASSERT_TRUE(fb_shw_vert != nullptr);
        std::string str_shw_vert(fb_shw_vert);
        ASSERT_TRUE(str_shw_vert.find("lightSpaceMatrix") != std::string::npos);

        const char* fb_shw_frag = ShaderManager::getFallbackShadowFragmentShader();
        ASSERT_TRUE(fb_shw_frag != nullptr);
        std::string str_shw_frag(fb_shw_frag);
        ASSERT_TRUE(str_shw_frag.find("main") != std::string::npos);
    }

    // =========================================================================
    // Test 4: Robust Non-Crashing Error Handling (Null/Empty Inputs)
    // =========================================================================
    {
        std::string err;

        // Null shader source handling
        GLuint null_shader = ShaderManager::compileShader(GL_VERTEX_SHADER, nullptr, &err);
        ASSERT_TRUE(null_shader == 0);
        ASSERT_FALSE(err.empty());

        // Empty shader source handling
        err.clear();
        GLuint empty_shader = ShaderManager::compileShader(GL_FRAGMENT_SHADER, "", &err);
        ASSERT_TRUE(empty_shader == 0);
        ASSERT_FALSE(err.empty());

        // Invalid shader handles passed to linkProgram
        err.clear();
        GLuint linked = ShaderManager::linkProgram(0, 0, &err);
        ASSERT_TRUE(linked == 0);
        ASSERT_FALSE(err.empty());

        // compileAndLink with null inputs
        err.clear();
        GLuint prog = ShaderManager::compileAndLink(nullptr, nullptr, &err);
        ASSERT_TRUE(prog == 0);
        ASSERT_FALSE(err.empty());
    }

    // =========================================================================
    // Test 5: Offscreen GL Compilation & Atomic Hot-Reload (If Context Available)
    // =========================================================================
    {
        bool gl_initialized = false;
        GLFWwindow* test_window = nullptr;

        if (glfwInit()) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            test_window = glfwCreateWindow(32, 32, "TestContext", nullptr, nullptr);
            if (test_window) {
                glfwMakeContextCurrent(test_window);
                if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                    gl_initialized = true;
                }
            }
        }

        if (gl_initialized) {
            // A. Valid Compilation & Linking
            std::string err;
            const char* vsrc =
                "#version 330 core\n"
                "layout(location = 0) in vec3 pos;\n"
                "void main() { gl_Position = vec4(pos, 1.0); }\n";
            const char* fsrc =
                "#version 330 core\n"
                "out vec4 fragColor;\n"
                "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";

            GLuint valid_prog = ShaderManager::compileAndLink(vsrc, fsrc, &err);
            ASSERT_TRUE(valid_prog != 0);
            ASSERT_TRUE(err.empty());

            // B. Non-Crashing Compilation Error Handling (Syntax Error)
            const char* bad_vsrc =
                "#version 330 core\n"
                "this is illegal syntax that will fail compilation;\n";

            err.clear();
            GLuint bad_shader = ShaderManager::compileShader(GL_VERTEX_SHADER, bad_vsrc, &err);
            ASSERT_TRUE(bad_shader == 0);
            ASSERT_FALSE(err.empty());

            // C. Atomic Hot-Reload Failure: Old Program Intact
            GLuint active_prog = valid_prog;
            err.clear();
            bool reload_ok = ShaderManager::atomicReload(active_prog, bad_vsrc, fsrc, &err);
            ASSERT_FALSE(reload_ok);
            // Verify active program handle was not destroyed or replaced
            ASSERT_TRUE(active_prog == valid_prog);
            ASSERT_FALSE(err.empty());

            // D. Atomic Hot-Reload Success
            const char* new_fsrc =
                "#version 330 core\n"
                "out vec4 fragColor;\n"
                "void main() { fragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n";

            err.clear();
            bool reload_success = ShaderManager::atomicReload(active_prog, vsrc, new_fsrc, &err);
            ASSERT_TRUE(reload_success);
            ASSERT_TRUE(active_prog != 0);
            ASSERT_TRUE(active_prog != valid_prog); // Replaced with new candidate

            // Clean up
            glDeleteProgram(active_prog);
        }

        if (test_window) {
            glfwDestroyWindow(test_window);
        }
        glfwTerminate();
    }

    // =========================================================================
    // Test 6: ShaderManager Registration & Query State Machine
    // =========================================================================
    {
        ShaderManager sm(&asset_mgr);
        ASSERT_TRUE(sm.getAssetManager() == &asset_mgr);
        ASSERT_FALSE(sm.hasProgram("standard"));
        ASSERT_TRUE(sm.getProgram("standard") == 0);
    }

    return failures;
}
