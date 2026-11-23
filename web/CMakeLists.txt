cmake_minimum_required(VERSION 3.28)

project(droidgfxsim_wasm LANGUAGES C CXX VERSION 1.0.0)

# Resolve repository root
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common compiler options
add_compile_options(-Wall -Wextra -Wno-warn-absolute-paths)

# Asset preloading path resolution
set(ASSETS_DIR "${REPO_ROOT}/app/src/main/assets")
if(NOT EXISTS "${ASSETS_DIR}")
    message(FATAL_ERROR "Assets directory not found at: ${ASSETS_DIR}")
endif()

# Dependencies: GLM
if(EXISTS "${REPO_ROOT}/dependencies/glm/glm/glm.hpp")
    set(GLM_INCLUDE_DIRS "${REPO_ROOT}/dependencies/glm")
else()
    include(FetchContent)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.3
    )
    FetchContent_MakeAvailable(glm)
    set(GLM_INCLUDE_DIRS "${glm_SOURCE_DIR}")
endif()

# Dependencies: tinyobjloader
if(EXISTS "${REPO_ROOT}/dependencies/tinyobjloader/tiny_obj_loader.h")
    set(TINYOBJLOADER_INCLUDE_DIRS "${REPO_ROOT}/dependencies/tinyobjloader")
else()
    include(FetchContent)
    FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG v2.0.0rc13
    )
    FetchContent_MakeAvailable(tinyobjloader)
    set(TINYOBJLOADER_INCLUDE_DIRS "${tinyobjloader_SOURCE_DIR}")
endif()

# Dependencies: Bullet Physics
set(BUILD_BULLET3 OFF CACHE BOOL "Disable Bullet3 target" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Disable Bullet unit tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static Bullet libs" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "Disable Bullet demos" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "Disable Bullet OpenGL demos" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "Disable Bullet2 demos" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "Disable Bullet extras" FORCE)
set(BUILD_DEMOS OFF CACHE BOOL "Disable Bullet all demos" FORCE)
set(INSTALL_LIBS OFF CACHE BOOL "Disable Bullet install" FORCE)

if(EXISTS "${REPO_ROOT}/dependencies/bullet/CMakeLists.txt")
    add_subdirectory("${REPO_ROOT}/dependencies/bullet" "${CMAKE_BINARY_DIR}/dependencies/bullet")
    set(BULLET_INCLUDE_DIRS "${REPO_ROOT}/dependencies/bullet/src")
else()
    include(FetchContent)
    FetchContent_Declare(
        bullet3
        GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
        GIT_TAG 3.25
    )
    FetchContent_MakeAvailable(bullet3)
    set(BULLET_INCLUDE_DIRS "${bullet3_SOURCE_DIR}/src")
endif()

# Source files (GLAD is intentionally excluded for WebAssembly/WebGL2)
set(WASM_SOURCES
    "${REPO_ROOT}/native/src/common/MathUtil.cc"
    "${REPO_ROOT}/native/src/graphics/Frustum.cc"
    "${REPO_ROOT}/native/src/graphics/camera.cc"
    "${REPO_ROOT}/native/src/graphics/gl_code.cc"
    "${REPO_ROOT}/native/src/graphics/gl2_renderer.cc"
    "${REPO_ROOT}/native/src/graphics/gl3_renderer.cc"
    "${REPO_ROOT}/native/src/graphics/scene_graph.cc"
    "${REPO_ROOT}/native/src/graphics/wavefront_factory.cc"
    "${REPO_ROOT}/native/src/physics/simulation.cc"
    "${REPO_ROOT}/native/src/application/application.cc"
    "${REPO_ROOT}/native/src/main.cc"
)

# Staged module sources (conditional inclusion)
if(EXISTS "${REPO_ROOT}/native/src/graphics/imgui_overlay.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/graphics/imgui_overlay.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/bullet_debug_drawer.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/graphics/bullet_debug_drawer.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/shader_manager.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/graphics/shader_manager.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/shadow_map.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/graphics/shadow_map.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/physics/raycast_picker.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/physics/raycast_picker.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/physics/projectile_spawner.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/physics/projectile_spawner.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/scene_presets.cc")
    list(APPEND WASM_SOURCES "${REPO_ROOT}/native/src/graphics/scene_presets.cc")
endif()

add_executable(droidgfxsim ${WASM_SOURCES})

target_include_directories(droidgfxsim PRIVATE
    "${REPO_ROOT}/native/include"
    "${REPO_ROOT}/native/include/common"
    "${REPO_ROOT}/native/include/graphics"
    "${REPO_ROOT}/native/include/physics"
    "${REPO_ROOT}/native/include/application"
    ${GLM_INCLUDE_DIRS}
    ${TINYOBJLOADER_INCLUDE_DIRS}
    ${BULLET_INCLUDE_DIRS}
)

target_compile_definitions(droidgfxsim PRIVATE
    EMSCRIPTEN=1
    DESKTOP_APP=1
    GLM_ENABLE_EXPERIMENTAL=1
)

target_link_libraries(droidgfxsim PRIVATE
    BulletDynamics
    BulletCollision
    LinearMath
)

# WebAssembly & Emscripten Linker Flags
target_link_options(droidgfxsim PRIVATE
    "-sUSE_GLFW=3"
    "-sFULL_ES3=1"
    "-sUSE_WEBGL2=1"
    "-sWASM=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "--preload-file" "${ASSETS_DIR}@/assets"
    "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

set_target_properties(droidgfxsim PROPERTIES
    OUTPUT_NAME "droidgfxsim"
    SUFFIX ".html"
)
