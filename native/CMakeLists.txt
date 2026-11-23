# Copyright (C) 2017-2026 Chris Liebert
cmake_minimum_required(VERSION 3.28)
project(droidgfxsim_forwarder NONE)

# Forward to root CMakeLists.txt using project root directory
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${PROJECT_ROOT}/CMakeLists.txt")
