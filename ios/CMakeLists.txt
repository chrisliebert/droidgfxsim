cmake_minimum_required(VERSION 3.28)

project(DroidGfxSim LANGUAGES C CXX OBJC OBJCXX VERSION 1.0.0)

# Resolve repository root
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect asset bundle resources
file(GLOB_RECURSE ASSET_FILES
    "${REPO_ROOT}/app/src/main/assets/*"
)

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

# Application and native sources (GLAD excluded for iOS OpenGLES 3.0)
set(IOS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/DroidGfxSim/AppDelegate.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/DroidGfxSim/ViewController.mm"
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
)

# Staged module sources (conditional inclusion)
if(EXISTS "${REPO_ROOT}/native/src/graphics/imgui_overlay.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/graphics/imgui_overlay.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/bullet_debug_drawer.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/graphics/bullet_debug_drawer.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/shader_manager.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/graphics/shader_manager.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/shadow_map.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/graphics/shadow_map.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/physics/raycast_picker.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/physics/raycast_picker.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/physics/projectile_spawner.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/physics/projectile_spawner.cc")
endif()
if(EXISTS "${REPO_ROOT}/native/src/graphics/scene_presets.cc")
    list(APPEND IOS_SOURCES "${REPO_ROOT}/native/src/graphics/scene_presets.cc")
endif()

add_executable(DroidGfxSim MACOSX_BUNDLE ${IOS_SOURCES} ${ASSET_FILES})

# Group assets in Xcode resource navigator
set_source_files_properties(${ASSET_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

set_target_properties(DroidGfxSim PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/DroidGfxSim/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.droidgfxsim.app"
    MACOSX_BUNDLE_BUNDLE_NAME "DroidGfxSim"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    RESOURCE "${ASSET_FILES}"
)

target_include_directories(DroidGfxSim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/DroidGfxSim"
    "${REPO_ROOT}/native/include"
    "${REPO_ROOT}/native/include/common"
    "${REPO_ROOT}/native/include/graphics"
    "${REPO_ROOT}/native/include/physics"
    "${REPO_ROOT}/native/include/application"
    ${GLM_INCLUDE_DIRS}
    ${TINYOBJLOADER_INCLUDE_DIRS}
    ${BULLET_INCLUDE_DIRS}
)

target_compile_definitions(DroidGfxSim PRIVATE
    IOS=1
    TARGET_OS_IPHONE=1
    GLM_ENABLE_EXPERIMENTAL=1
)

# Framework Dependencies
find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
find_library(OPENGLES_FRAMEWORK OpenGLES REQUIRED)
find_library(GLKIT_FRAMEWORK GLKit REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)

target_link_libraries(DroidGfxSim PRIVATE
    ${UIKIT_FRAMEWORK}
    ${OPENGLES_FRAMEWORK}
    ${GLKIT_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    ${COREGRAPHICS_FRAMEWORK}
    BulletDynamics
    BulletCollision
    LinearMath
)
