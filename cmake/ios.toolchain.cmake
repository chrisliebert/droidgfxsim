# iOS CMake Toolchain File
# Standard toolchain for building iOS applications and libraries with CMake and Xcode.

cmake_minimum_required(VERSION 3.28)

# Target operating system
set(CMAKE_SYSTEM_NAME iOS)

# Options: OS (Device), SIMULATOR (Simulator arm64/x86_64), SIMULATORARM64, SIMULATOR64
set(IOS_PLATFORM "OS" CACHE STRING "Type of iOS build target: OS, SIMULATOR, SIMULATORARM64, SIMULATOR64")

# iOS Deployment Target
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS deployment version")

# Resolve Platform SDK & Architectures
if(IOS_PLATFORM STREQUAL "OS")
    set(IOS_SDK_NAME "iphoneos")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for iOS Device" FORCE)
elseif(IOS_PLATFORM STREQUAL "SIMULATOR")
    set(IOS_SDK_NAME "iphonesimulator")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for iOS Simulator" FORCE)
elseif(IOS_PLATFORM STREQUAL "SIMULATORARM64")
    set(IOS_SDK_NAME "iphonesimulator")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture for Apple Silicon Simulator" FORCE)
elseif(IOS_PLATFORM STREQUAL "SIMULATOR64")
    set(IOS_SDK_NAME "iphonesimulator")
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architecture for Intel Simulator" FORCE)
else()
    message(FATAL_ERROR "Unsupported IOS_PLATFORM: ${IOS_PLATFORM}. Valid options: OS, SIMULATOR, SIMULATORARM64, SIMULATOR64")
endif()

# Locate SDK Path via xcrun when running on macOS host
if(CMAKE_HOST_APPLE)
    execute_process(
        COMMAND xcrun --sdk ${IOS_SDK_NAME} --show-sdk-path
        OUTPUT_VARIABLE IOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(EXISTS "${IOS_SDK_PATH}")
        set(CMAKE_OSX_SYSROOT "${IOS_SDK_PATH}" CACHE PATH "Path to iOS SDK" FORCE)
    else()
        set(CMAKE_OSX_SYSROOT "${IOS_SDK_NAME}" CACHE PATH "Path to iOS SDK" FORCE)
    endif()
else()
    set(CMAKE_OSX_SYSROOT "${IOS_SDK_NAME}" CACHE PATH "Path to iOS SDK" FORCE)
endif()

# Cross-compilation root path settings
set(CMAKE_FIND_ROOT_PATH "${CMAKE_OSX_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Disable code signing by default for headless/automated CI builds
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")

# Define preprocessor macros for iOS targets
add_compile_definitions(IOS=1 TARGET_OS_IPHONE=1)

message(STATUS "Configured iOS Toolchain: Platform=${IOS_PLATFORM}, SDK=${IOS_SDK_NAME}, Arch=${CMAKE_OSX_ARCHITECTURES}")
