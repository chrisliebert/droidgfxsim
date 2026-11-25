cmake_minimum_required(VERSION 3.28)

# Allow dependencies (e.g. Bullet) with older cmake policies to configure with modern CMake
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# Ensure uniform library naming across all configurations without cache bleed
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
set(CMAKE_MINSIZEREL_POSTFIX "" CACHE STRING "" FORCE)
set(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "" FORCE)

project(droidgfxsim LANGUAGES C CXX VERSION 1.0.0)

# Resolve project root directory reliably regardless of whether configured from root or native/
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep FetchContent artifacts inside build directory
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "FetchContent base directory" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED OFF CACHE BOOL "Allow FetchContent to update" FORCE)

include(FetchContent)
include(CTest)

# Global compiler configuration
if(MSVC)
    add_compile_options(/W4 /FS /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# OpenGL package discovery (Desktop platforms)
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT IOS)
    find_package(OpenGL REQUIRED)
endif()

# Vulkan package discovery
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
endif()

# Dependency: GLM (submodule or FetchContent)
if(EXISTS "${PROJECT_ROOT}/dependencies/glm/glm/glm.hpp")
    set(GLM_INCLUDE_DIRS "${PROJECT_ROOT}/dependencies/glm")
    message(STATUS "Using GLM submodule from ${GLM_INCLUDE_DIRS}")
else()
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.3
    )
    FetchContent_MakeAvailable(glm)
    set(GLM_INCLUDE_DIRS "${glm_SOURCE_DIR}")
endif()

# Dependency: tinyobjloader (submodule or FetchContent)
if(EXISTS "${PROJECT_ROOT}/dependencies/tinyobjloader/tiny_obj_loader.h")
    set(TINYOBJLOADER_INCLUDE_DIRS "${PROJECT_ROOT}/dependencies/tinyobjloader")
    message(STATUS "Using tinyobjloader submodule from ${TINYOBJLOADER_INCLUDE_DIRS}")
else()
    FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG v2.0.0rc13
    )
    FetchContent_MakeAvailable(tinyobjloader)
    set(TINYOBJLOADER_INCLUDE_DIRS "${tinyobjloader_SOURCE_DIR}")
endif()

# Dependency: GLFW (submodule or FetchContent, Desktop only)
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT IOS)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Disable GLFW examples" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Disable GLFW tests" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Disable GLFW docs" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Disable GLFW install" FORCE)

    if(EXISTS "${PROJECT_ROOT}/dependencies/glfw/CMakeLists.txt")
        message(STATUS "Using GLFW submodule from dependencies/glfw")
        add_subdirectory("${PROJECT_ROOT}/dependencies/glfw" "${CMAKE_BINARY_DIR}/dependencies/glfw")
    else()
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.5.1
        )
        FetchContent_MakeAvailable(glfw)
    endif()
endif()

# Dependency: Bullet Physics (submodule or FetchContent)
set(BUILD_BULLET3 OFF CACHE BOOL "Disable Bullet3 target" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Disable Bullet unit tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static Bullet libs" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "Disable Bullet demos" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "Disable Bullet OpenGL demos" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "Disable Bullet2 demos" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "Disable Bullet extras" FORCE)
set(BUILD_DEMOS OFF CACHE BOOL "Disable Bullet all demos" FORCE)
set(INSTALL_LIBS ON CACHE BOOL "Enable Bullet install libs" FORCE)
if(MSVC)
    set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "" FORCE)
endif()

if(EXISTS "${PROJECT_ROOT}/dependencies/bullet/CMakeLists.txt")
    message(STATUS "Using Bullet submodule from dependencies/bullet")
    add_subdirectory("${PROJECT_ROOT}/dependencies/bullet" "${CMAKE_BINARY_DIR}/dependencies/bullet")
    set(BULLET_INCLUDE_DIRS "${PROJECT_ROOT}/dependencies/bullet/src")
else()
    FetchContent_Declare(
        bullet3
        GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
        GIT_TAG 3.25
    )
    FetchContent_MakeAvailable(bullet3)
    set(BULLET_INCLUDE_DIRS "${bullet3_SOURCE_DIR}/src")
endif()

# Dependency: Dear ImGui (Docking / C++20)
if(EXISTS "${PROJECT_ROOT}/dependencies/imgui/imgui.h")
    set(IMGUI_DIR "${PROJECT_ROOT}/dependencies/imgui")
    message(STATUS "Using Dear ImGui submodule from ${IMGUI_DIR}")
else()
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.5
    )
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
        set(IMGUI_DIR "${imgui_SOURCE_DIR}")
    endif()
endif()

if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT IOS AND IMGUI_DIR)
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui SYSTEM PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
    target_compile_features(imgui PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(imgui PRIVATE /W3)
    endif()
endif()

# Common core library sources
set(CORE_SOURCES
    "${PROJECT_ROOT}/native/src/common/MathUtil.cc"
    "${PROJECT_ROOT}/native/src/graphics/Frustum.cc"
    "${PROJECT_ROOT}/native/src/graphics/camera.cc"
    "${PROJECT_ROOT}/native/src/graphics/gl_code.cc"
    "${PROJECT_ROOT}/native/src/graphics/gl2_renderer.cc"
    "${PROJECT_ROOT}/native/src/graphics/gl3_renderer.cc"
    "${PROJECT_ROOT}/native/src/graphics/scene_graph.cc"
    "${PROJECT_ROOT}/native/src/graphics/wavefront_factory.cc"
    "${PROJECT_ROOT}/native/src/physics/simulation.cc"
    "${PROJECT_ROOT}/native/src/application/application.cc"
)

# Desktop GLAD OpenGL loader (excluded on Android, Emscripten, and iOS)
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT IOS)
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/desktop/glad.c")
endif()

# Lane L2 optional source modules (staged cleanly via if(EXISTS ...))
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/imgui_overlay.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/imgui_overlay.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/bullet_debug_drawer.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/bullet_debug_drawer.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/shader_manager.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/shader_manager.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/shadow_map.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/shadow_map.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/vulkan_renderer.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/vulkan_renderer.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/particle_system.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/particle_system.cc")
endif()

# Lane L3 optional source modules (staged cleanly via if(EXISTS ...))
if(EXISTS "${PROJECT_ROOT}/native/src/physics/raycast_picker.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/physics/raycast_picker.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/physics/projectile_spawner.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/physics/projectile_spawner.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/graphics/scene_presets.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/graphics/scene_presets.cc")
endif()
if(EXISTS "${PROJECT_ROOT}/native/src/physics/terrain.cc")
    list(APPEND CORE_SOURCES "${PROJECT_ROOT}/native/src/physics/terrain.cc")
endif()

add_library(droidgfxsim_core STATIC ${CORE_SOURCES})

target_include_directories(droidgfxsim_core PUBLIC
    "${PROJECT_ROOT}/native/include"
    "${PROJECT_ROOT}/native/include/common"
    "${PROJECT_ROOT}/native/include/graphics"
    "${PROJECT_ROOT}/native/include/physics"
    "${PROJECT_ROOT}/native/include/application"
    ${GLM_INCLUDE_DIRS}
    ${TINYOBJLOADER_INCLUDE_DIRS}
    ${BULLET_INCLUDE_DIRS}
)

if(IMGUI_DIR)
    target_include_directories(droidgfxsim_core PUBLIC
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"
    )
endif()

target_compile_features(droidgfxsim_core PUBLIC cxx_std_20)
target_compile_definitions(droidgfxsim_core PUBLIC GLM_ENABLE_EXPERIMENTAL=1)

if(Vulkan_FOUND)
    target_compile_definitions(droidgfxsim_core PUBLIC DROIDGFXSIM_HAS_VULKAN=1)
    if(Vulkan_INCLUDE_DIRS)
        target_include_directories(droidgfxsim_core PUBLIC ${Vulkan_INCLUDE_DIRS})
    endif()
endif()

if(NOT ANDROID)
    target_include_directories(droidgfxsim_core PUBLIC "${PROJECT_ROOT}/native/include/desktop")
    target_compile_definitions(droidgfxsim_core PUBLIC DESKTOP_APP=1)
    target_link_libraries(droidgfxsim_core PUBLIC
        OpenGL::GL
        glfw
        BulletDynamics
        BulletCollision
        LinearMath
    )
    if(TARGET imgui)
        target_link_libraries(droidgfxsim_core PUBLIC imgui)
    endif()
    if(Vulkan_FOUND)
        if(TARGET Vulkan::Vulkan)
            target_link_libraries(droidgfxsim_core PUBLIC Vulkan::Vulkan)
        elseif(Vulkan_LIBRARIES)
            target_link_libraries(droidgfxsim_core PUBLIC ${Vulkan_LIBRARIES})
        endif()
    endif()
else()
    target_include_directories(droidgfxsim_core PUBLIC "${PROJECT_ROOT}/native/include/android")
    target_link_libraries(droidgfxsim_core PUBLIC
        GLESv3
        EGL
        android
        log
        m
        BulletDynamics
        BulletCollision
        LinearMath
    )
endif()

# Desktop executable
if(NOT ANDROID)
    add_executable(desktop_app "${PROJECT_ROOT}/native/src/main.cc")
    target_link_libraries(desktop_app PRIVATE droidgfxsim_core)
    target_compile_features(desktop_app PRIVATE cxx_std_20)
    target_compile_definitions(desktop_app PRIVATE DESKTOP_APP=1)
    set_target_properties(desktop_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(MSVC)
        target_link_options(desktop_app PRIVATE /STACK:8388608)
    endif()
endif()

# Unit tests
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory("${PROJECT_ROOT}/tests" "${CMAKE_BINARY_DIR}/tests")
endif()
