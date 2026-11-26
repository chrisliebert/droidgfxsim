# Modernized Android NDK CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

# Allow dependencies with older cmake policies to configure
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(glappjni LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../../../../native")
set(ROOT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")

# Dependencies (submodules)
set(GLM_INCLUDE_DIRS "${ROOT_LOCATION}/dependencies/glm")
set(TINYOBJLOADER_INCLUDE_DIRS "${ROOT_LOCATION}/dependencies/tinyobjloader")

# Bullet Physics via submodule or FetchContent
set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(INSTALL_LIBS ON CACHE BOOL "" FORCE)

if(EXISTS "${ROOT_LOCATION}/dependencies/bullet/CMakeLists.txt")
    add_subdirectory("${ROOT_LOCATION}/dependencies/bullet" bullet-build)
    set(BULLET_INCLUDE_DIRS "${ROOT_LOCATION}/dependencies/bullet/src")
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

set(JNI_SOURCES
    ${SRC_LOCATION}/src/android/gl3stub_wrapper.c
    ${SRC_LOCATION}/src/android/glappjni.cc
    ${SRC_LOCATION}/src/common/MathUtil.cc
    ${SRC_LOCATION}/src/graphics/Frustum.cc
    ${SRC_LOCATION}/src/graphics/camera.cc
    ${SRC_LOCATION}/src/graphics/gl_code.cc
    ${SRC_LOCATION}/src/graphics/gl2_renderer.cc
    ${SRC_LOCATION}/src/graphics/gl3_renderer.cc
    ${SRC_LOCATION}/src/graphics/scene_graph.cc
    ${SRC_LOCATION}/src/graphics/wavefront_factory.cc
    ${SRC_LOCATION}/src/physics/simulation.cc
    ${SRC_LOCATION}/src/application/application.cc
)

if(EXISTS "${SRC_LOCATION}/src/physics/terrain.cc")
    list(APPEND JNI_SOURCES "${SRC_LOCATION}/src/physics/terrain.cc")
endif()

add_library(glappjni SHARED ${JNI_SOURCES})

target_include_directories(glappjni PRIVATE
    ${SRC_LOCATION}/include
    ${SRC_LOCATION}/include/common
    ${SRC_LOCATION}/include/graphics
    ${SRC_LOCATION}/include/application
    ${SRC_LOCATION}/include/android
    ${SRC_LOCATION}/include/physics
    ${GLM_INCLUDE_DIRS}
    ${TINYOBJLOADER_INCLUDE_DIRS}
    ${BULLET_INCLUDE_DIRS}
)

target_compile_definitions(glappjni PRIVATE
    GLM_ENABLE_EXPERIMENTAL=1
    __ANDROID__=1
)

target_link_libraries(glappjni PRIVATE
    GLESv3
    EGL
    android
    log
    m
    BulletDynamics
    BulletCollision
    LinearMath
)
