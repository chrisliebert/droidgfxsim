// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/shader_manager.h"
#include <iostream>

ShaderManager::ShaderManager(AssetManager* asset_mgr)
    : asset_manager(asset_mgr), owns_asset_manager(false) {
    if (!asset_manager) {
#if !defined(__ANDROID__)
        asset_manager = new AssetManager();
        owns_asset_manager = true;
#endif
    }
}

ShaderManager::~ShaderManager() {
    for (auto& pair : programs) {
        if (pair.second.program_id != 0) {
            glDeleteProgram(pair.second.program_id);
            pair.second.program_id = 0;
        }
    }
    programs.clear();
    if (owns_asset_manager && asset_manager) {
        delete asset_manager;
        asset_manager = nullptr;
    }
}

void ShaderManager::setAssetManager(AssetManager* mgr) {
    if (owns_asset_manager && asset_manager) {
        delete asset_manager;
        owns_asset_manager = false;
    }
    asset_manager = mgr;
}

GLuint ShaderManager::compileShader(GLenum type, const char* source, std::string* error_out) {
    if (!source || source[0] == '\0') {
        const char* msg = "Empty shader source provided";
        LOGE("%s", msg);
        if (error_out) *error_out = msg;
        return 0;
    }

    GLuint shader = glCreateShader(type);
    if (!shader) {
        const char* msg = "Failed to allocate OpenGL shader object";
        LOGE("%s", msg);
        if (error_out) *error_out = msg;
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        std::string info_log;
        if (info_len > 1) {
            std::vector<char> info(info_len);
            glGetShaderInfoLog(shader, info_len, nullptr, info.data());
            info_log = std::string(info.data());
        } else {
            info_log = "Unknown shader compilation error";
        }

        const char* type_str = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
        LOGE("%s shader compilation failed:\n%s", type_str, info_log.c_str());
        if (error_out) {
            *error_out = std::string(type_str) + " shader compilation failed: " + info_log;
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ShaderManager::linkProgram(GLuint vert_shader, GLuint frag_shader, std::string* error_out) {
    if (!vert_shader || !frag_shader) {
        const char* msg = "Invalid shader handles passed to linkProgram";
        LOGE("%s", msg);
        if (error_out) *error_out = msg;
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        const char* msg = "Failed to allocate OpenGL program object";
        LOGE("%s", msg);
        if (error_out) *error_out = msg;
        return 0;
    }

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        std::string info_log;
        if (info_len > 1) {
            std::vector<char> info(info_len);
            glGetProgramInfoLog(program, info_len, nullptr, info.data());
            info_log = std::string(info.data());
        } else {
            info_log = "Unknown program linking error";
        }

        LOGE("Shader program linking failed:\n%s", info_log.c_str());
        if (error_out) {
            *error_out = "Shader program linking failed: " + info_log;
        }

        glDetachShader(program, vert_shader);
        glDetachShader(program, frag_shader);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

GLuint ShaderManager::compileAndLink(const char* vertex_src, const char* fragment_src, std::string* error_out) {
    std::string err;
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertex_src, &err);
    if (!vert) {
        if (error_out) *error_out = err;
        return 0;
    }

    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragment_src, &err);
    if (!frag) {
        glDeleteShader(vert);
        if (error_out) *error_out = err;
        return 0;
    }

    GLuint prog = linkProgram(vert, frag, &err);
    glDeleteShader(vert);
    glDeleteShader(frag);

    if (!prog && error_out) {
        *error_out = err;
    }
    return prog;
}

bool ShaderManager::atomicReload(GLuint& inout_program,
                                 const std::string& vertex_src,
                                 const std::string& fragment_src,
                                 std::string* error_out) {
    std::string err;
    GLuint candidate = compileAndLink(vertex_src.c_str(), fragment_src.c_str(), &err);
    if (!candidate) {
        LOGE("Atomic hot-reload rejected candidate shaders: %s", err.c_str());
        if (error_out) *error_out = err;
        // Existing inout_program remains completely intact and valid!
        return false;
    }

    // Atomic swap: replace old program and free resources
    GLuint old_program = inout_program;
    inout_program = candidate;
    if (old_program != 0) {
        glDeleteProgram(old_program);
    }

    LOGI("Atomic hot-reload succeeded: program %u replaced with %u", old_program, candidate);
    return true;
}

GLuint ShaderManager::loadProgram(const std::string& name,
                                  const std::string& vertex_path,
                                  const std::string& fragment_path,
                                  const char* fallback_vert,
                                  const char* fallback_frag) {
    ManagedProgram entry;
    entry.name = name;
    entry.vertex_path = vertex_path;
    entry.fragment_path = fragment_path;
    entry.fallback_vertex_src = fallback_vert ? fallback_vert : getFallbackStandardVertexShader();
    entry.fallback_fragment_src = fallback_frag ? fallback_frag : getFallbackStandardFragmentShader();

    std::string vert_content;
    std::string frag_content;

    if (asset_manager) {
        vert_content = asset_manager->loadTextFile(vertex_path.c_str());
        frag_content = asset_manager->loadTextFile(fragment_path.c_str());
    }

    bool loaded_from_file = (!vert_content.empty() && !frag_content.empty());
    const char* vsrc = loaded_from_file ? vert_content.c_str() : entry.fallback_vertex_src.c_str();
    const char* fsrc = loaded_from_file ? frag_content.c_str() : entry.fallback_fragment_src.c_str();

    std::string err;
    GLuint prog = compileAndLink(vsrc, fsrc, &err);
    if (!prog && loaded_from_file) {
        LOGI("Warning: Failed compiling shaders from %s/%s; attempting fallback: %s",
             vertex_path.c_str(), fragment_path.c_str(), err.c_str());
        prog = compileAndLink(entry.fallback_vertex_src.c_str(), entry.fallback_fragment_src.c_str(), &err);
        loaded_from_file = false;
    }

    if (!prog) {
        last_error = "Failed to load program '" + name + "': " + err;
        LOGE("%s", last_error.c_str());
        return 0;
    }

    entry.program_id = prog;
    entry.loaded_from_file = loaded_from_file;
    programs[name] = entry;
    LOGI("Program '%s' loaded successfully (ID: %u, file-based: %s)",
         name.c_str(), prog, loaded_from_file ? "true" : "false");
    return prog;
}

bool ShaderManager::reloadProgram(const std::string& name, std::string* error_out) {
    auto it = programs.find(name);
    if (it == programs.end()) {
        std::string err = "Program '" + name + "' is not registered";
        LOGE("%s", err.c_str());
        if (error_out) *error_out = err;
        return false;
    }

    ManagedProgram& entry = it->second;
    if (!asset_manager) {
        std::string err = "No AssetManager available to reload program '" + name + "'";
        LOGE("%s", err.c_str());
        if (error_out) *error_out = err;
        return false;
    }

    std::string vsrc = asset_manager->loadTextFile(entry.vertex_path.c_str());
    std::string fsrc = asset_manager->loadTextFile(entry.fragment_path.c_str());

    if (vsrc.empty() || fsrc.empty()) {
        std::string err = "Failed reading shader files for reload: " + entry.vertex_path + " / " + entry.fragment_path;
        LOGE("%s", err.c_str());
        if (error_out) *error_out = err;
        return false;
    }

    std::string reload_err;
    bool ok = atomicReload(entry.program_id, vsrc, fsrc, &reload_err);
    if (!ok) {
        last_error = reload_err;
        if (error_out) *error_out = reload_err;
        return false;
    }

    entry.loaded_from_file = true;
    return true;
}

bool ShaderManager::reloadAll(std::string* error_out) {
    bool all_ok = true;
    std::string aggregated_err;
    for (auto& pair : programs) {
        std::string err;
        if (!reloadProgram(pair.first, &err)) {
            all_ok = false;
            aggregated_err += "[" + pair.first + ": " + err + "] ";
        }
    }
    if (!all_ok && error_out) {
        *error_out = aggregated_err;
    }
    return all_ok;
}

GLuint ShaderManager::getProgram(const std::string& name) const {
    auto it = programs.find(name);
    if (it != programs.end()) {
        return it->second.program_id;
    }
    return 0;
}

bool ShaderManager::hasProgram(const std::string& name) const {
    return programs.find(name) != programs.end();
}

const char* ShaderManager::getFallbackStandardVertexShader() {
    return "#version 300 es\n"
           "layout(location = 0) in vec3 vPosition;\n"
           "layout(location = 1) in vec3 vNormal;\n"
           "layout(location = 2) in vec2 vTexCoord;\n"
           "layout (std140) uniform TransformBlock {\n"
           "    mat4 projection;\n"
           "    mat4 modelview;\n"
           "};\n"
           "uniform mat4 matrix;\n"
           "uniform mat4 lightSpaceMatrix;\n"
           "uniform vec3 lightPosition;\n"
           "out vec3 fragPos;\n"
           "out vec3 worldPos;\n"
           "out vec3 normal;\n"
           "out vec2 texcoord;\n"
           "out vec3 lightPos;\n"
           "out vec4 fragPosLightSpace;\n"
           "void main() {\n"
           "    vec4 worldPosition = matrix * vec4(vPosition, 1.0);\n"
           "    worldPos = worldPosition.xyz;\n"
           "    gl_Position = projection * modelview * worldPosition;\n"
           "    fragPos = vec3(modelview * worldPosition);\n"
           "    normal = mat3(transpose(inverse(modelview * matrix))) * vNormal;\n"
           "    vec3 lightPosIn = (length(lightPosition) > 0.001) ? lightPosition : vec3(20.0, 40.0, 20.0);\n"
           "    lightPos = vec3(modelview * vec4(lightPosIn, 1.0));\n"
           "    texcoord = vTexCoord;\n"
           "    fragPosLightSpace = lightSpaceMatrix * worldPosition;\n"
           "}\n";
}

const char* ShaderManager::getFallbackStandardFragmentShader() {
    return "#version 300 es\n"
           "precision mediump float;\n"
           "in vec3 fragPos;\n"
           "in vec3 worldPos;\n"
           "in vec3 normal;\n"
           "in vec2 texcoord;\n"
           "in vec3 lightPos;\n"
           "in vec4 fragPosLightSpace;\n"
           "out vec4 color;\n"
           "uniform sampler2D diffuseTexture;\n"
           "uniform sampler2D shadowMap;\n"
           "uniform float ambientStrength;\n"
           "uniform vec3 lightColor;\n"
           "uniform int shadowsEnabled;\n"
           "float calculateShadow(vec4 fragPosLight, vec3 norm, vec3 lightDir) {\n"
           "    vec3 projCoords = fragPosLight.xyz / fragPosLight.w;\n"
           "    projCoords = projCoords * 0.5 + 0.5;\n"
           "    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;\n"
           "    float bias = max(0.005 * (1.0 - dot(norm, lightDir)), 0.0005);\n"
           "    float shadow = 0.0;\n"
           "    vec2 mapSize = vec2(textureSize(shadowMap, 0));\n"
           "    vec2 texelSize = (mapSize.x > 0.0) ? (1.0 / mapSize) : vec2(1.0 / 2048.0);\n"
           "    for (int x = -1; x <= 1; ++x) {\n"
           "        for (int y = -1; y <= 1; ++y) {\n"
           "            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(float(x), float(y)) * texelSize).r;\n"
           "            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;\n"
           "        }\n"
           "    }\n"
           "    return shadow / 9.0;\n"
           "}\n"
           "void main() {\n"
           "    vec3 norm = normalize(normal);\n"
           "    vec3 lightDir = normalize(lightPos - fragPos);\n"
           "    float amb = (ambientStrength > 0.0) ? ambientStrength : 0.25;\n"
           "    vec3 lColor = (length(lightColor) > 0.001) ? lightColor : vec3(1.0, 1.0, 1.0);\n"
           "    vec3 ambient = amb * lColor;\n"
           "    float diff = max(dot(norm, lightDir), 0.0);\n"
           "    vec3 diffuse = diff * lColor;\n"
           "    float shadow = 0.0;\n"
           "    if (shadowsEnabled != 0) shadow = calculateShadow(fragPosLightSpace, norm, lightDir);\n"
           "    vec3 lighting = ambient + (1.0 - shadow) * diffuse;\n"
           "    color = texture(diffuseTexture, texcoord) * vec4(lighting, 1.0);\n"
           "}\n";
}

const char* ShaderManager::getFallbackShadowVertexShader() {
    return "#version 300 es\n"
           "layout(location = 0) in vec3 vPosition;\n"
           "uniform mat4 lightSpaceMatrix;\n"
           "uniform mat4 matrix;\n"
           "void main() {\n"
           "    gl_Position = lightSpaceMatrix * matrix * vec4(vPosition, 1.0);\n"
           "}\n";
}

const char* ShaderManager::getFallbackShadowFragmentShader() {
    return "#version 300 es\n"
           "precision highp float;\n"
           "void main() {}\n";
}
