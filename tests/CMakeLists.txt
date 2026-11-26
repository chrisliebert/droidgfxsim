# Tests configuration for droidgfxsim

set(TEST_SOURCES
    test_main.cc
    test_math.cc
    test_camera.cc
    test_frustum.cc
    test_scenenode.cc
    test_asset_manager.cc
    test_wavefront.cc
    test_physics.cc
    test_raycast.cc
    test_shadow_map.cc
    test_shader_manager.cc
)

if(NOT Vulkan_FOUND)
    find_package(Vulkan QUIET)
endif()

if(Vulkan_FOUND OR DROIDGFXSIM_HAS_VULKAN)
    list(APPEND TEST_SOURCES test_vulkan.cc)
endif()

add_executable(unit_tests ${TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE droidgfxsim_core)
target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(unit_tests PRIVATE cxx_std_20)

if(Vulkan_FOUND OR DROIDGFXSIM_HAS_VULKAN)
    target_compile_definitions(unit_tests PRIVATE DROIDGFXSIM_HAS_VULKAN=1)
    if(TARGET Vulkan::Vulkan)
        target_link_libraries(unit_tests PRIVATE Vulkan::Vulkan)
    elseif(Vulkan_LIBRARIES)
        target_link_libraries(unit_tests PRIVATE ${Vulkan_LIBRARIES})
    endif()
    if(Vulkan_INCLUDE_DIRS)
        target_include_directories(unit_tests PRIVATE ${Vulkan_INCLUDE_DIRS})
    endif()
endif()

add_test(
    NAME unit_tests
    COMMAND unit_tests
    WORKING_DIRECTORY "${PROJECT_ROOT}/app/src/main/assets"
)

add_custom_target(run_tests
    COMMAND unit_tests
    DEPENDS unit_tests
    WORKING_DIRECTORY "${PROJECT_ROOT}/app/src/main/assets"
    COMMENT "Executing droidgfxsim unit test suite..."
)
