// Copyright (C) 2017-2026 Chris Liebert

#ifndef _SHADER_MANAGER_H_
#define _SHADER_MANAGER_H_

#include "graphics/gl_code.h"
#include "common/asset_manager.hpp"
#include "common/log.h"
#include <string>
#include <unordered_map>
#include <vector>

class ShaderManager {
public:
    struct ManagedProgram {
        std::string name;
        std::string vertex_path;
        std::string fragment_path;
        std::string fallback_vertex_src;
        std::string fallback_fragment_src;
        GLuint program_id = 0;
        bool loaded_from_file = false;
    };

private:
    AssetManager* asset_manager = nullptr;
    bool owns_asset_manager = false;
    std::unordered_map<std::string, ManagedProgram> programs;
    std::string last_error;

public:
    ShaderManager(AssetManager* asset_mgr = nullptr);
    ~ShaderManager();

    void setAssetManager(AssetManager* mgr);
    AssetManager* getAssetManager() const { return asset_manager; }

    // Compile individual shader with error logging; returns 0 on failure
    static GLuint compileShader(GLenum type, const char* source, std::string* error_out = nullptr);

    // Link shader program with error logging; returns 0 on failure
    static GLuint linkProgram(GLuint vert_shader, GLuint frag_shader, std::string* error_out = nullptr);

    // Full compilation and linking pipeline with error inspection
    static GLuint compileAndLink(const char* vertex_src, const char* fragment_src, std::string* error_out = nullptr);

    // Atomic hot-reload swap on an active program handle without crashing on errors
    static bool atomicReload(GLuint& inout_program,
                             const std::string& vertex_src,
                             const std::string& fragment_src,
                             std::string* error_out = nullptr);

    // Load and register a managed shader program (uses file if available, otherwise fallback)
    GLuint loadProgram(const std::string& name,
                       const std::string& vertex_path,
                       const std::string& fragment_path,
                       const char* fallback_vert = nullptr,
                       const char* fallback_frag = nullptr);

    // Atomically reload a registered shader program from its asset files
    bool reloadProgram(const std::string& name, std::string* error_out = nullptr);

    // Atomically reload all registered shader programs
    bool reloadAll(std::string* error_out = nullptr);

    // Query active program ID by name
    GLuint getProgram(const std::string& name) const;

    // Check if program is registered
    bool hasProgram(const std::string& name) const;

    // Check last error message
    const std::string& getLastError() const { return last_error; }

    // Static fallback shader sources for standard rendering & shadow mapping
    static const char* getFallbackStandardVertexShader();
    static const char* getFallbackStandardFragmentShader();
    static const char* getFallbackShadowVertexShader();
    static const char* getFallbackShadowFragmentShader();
};

#endif // _SHADER_MANAGER_H_
