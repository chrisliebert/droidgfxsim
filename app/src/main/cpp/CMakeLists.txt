cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
# -fno-rtti -fno-exceptions

SET(USE_EXISTING_GLM OFF CACHE BOOL "use an existing glm installation")
SET(USE_EXISTING_TINYOBJLOADER OFF CACHE BOOL "use an existing tinybojloader installation")

SET(CMAKE_DEBUG_POSTFIX "" CACHE STRING "add a postfix for Debug mode")
SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug or Release build configuration")

SET(CMAKE_VERBOSE_MAKEFILE ON)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
  MESSAGE(Warning, "CMAKE_BUILD_TYPE not specified, defaulting to Debug. Note: switching the configuration after the dependencies are built will cause dependency problems. Consider using a separate directory for each CMake build configuration")
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")

IF("${CMAKE_BUILD_TYPE}" STREQUAL Release)
  SET(LIB_SUFFIX "")
ELSEIF("${CMAKE_BUILD_TYPE}" STREQUAL Debug)
  SET(LIB_SUFFIX "")	
ELSE("${CMAKE_BUILD_TYPE}" STREQUAL Release)
  MESSAGE(ERROR "CMAKE_BUILD_TYPE not recognized, use Debug or Release")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL Release)  

SET(LIB_PREFIX "lib")
SET(LIB_EXTENSION ".a")

FIND_PACKAGE(Git REQUIRED)

SET(TINYOBJLOADER_PREFIX ${CMAKE_BINARY_DIR}/tinyobjloader)
SET(GLM_PREFIX ${CMAKE_BINARY_DIR}/glm)

IF(NOT USE_EXISTING_GLM)
    FIND_PACKAGE(Git REQUIRED)
    SET(GLM_PREFIX ${CMAKE_BINARY_DIR}/glm)
    MESSAGE(" GLM not found, downloading via GIT")
    INCLUDE(ExternalProject)
    ExternalProject_Add(
      glm_dependency
      PREFIX ${GLM_PREFIX}
      GIT_REPOSITORY https://github.com/g-truc/glm.git
      CMAKE_GENERATOR "${CMAKE_GENERATOR}"
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:STRING=${GLM_PREFIX} -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER} -DCMAKE_MAKE_PROGRAM:STRING=${CMAKE_MAKE_PROGRAM} -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      #CONFIGURE_COMMAND ""
      #BUILD_COMMAND ""
      #INSTALL_COMMAND ""
      UPDATE_COMMAND "" #disable updating git repo every build
      LOG_DOWNLOAD ON
    )
    ExternalProject_Get_Property(glm_dependency install_dir)
    SET(GLM_INCLUDE_DIRS ${install_dir}/include)
    ADD_CUSTOM_COMMAND(
      OUTPUT
        ${install_dir}/include/glm/glm.hpp
        ${GLM_PREFIX}/src/glm_dependency-stamp/glm_dependency-gitinfo.txt
        ${GLM_PREFIX}/tmp/glm_dependency-cfgcmd.txt
      COMMAND
    )
    
    ADD_CUSTOM_TARGET(glmProvider DEPENDS
        ${install_dir}/include/glm/glm.hpp
        ${GLM_PREFIX}/src/glm_dependency-stamp/glm_dependency-gitinfo.txt
        ${GLM_PREFIX}/tmp/glm_dependency-cfgcmd.txt
    )
    SET(GLM_FOUND TRUE)
#ELSE(NOT USE_EXISTING_GLM)
    #FIND_PACKAGE(glm REQUIRED)
ENDIF(NOT USE_EXISTING_GLM)

IF(NOT USE_EXISTING_TINYOBJLOADER)
    FIND_PACKAGE(Git REQUIRED)
    SET(TINYOBJLOADER_PREFIX ${CMAKE_BINARY_DIR}/tinyobjloader)
    MESSAGE(" TINYOBJLOADER not found, downloading via GIT")
    INCLUDE(ExternalProject)
    #SET(GENERATOR_ARG -G\"${CMAKE_GENERATOR}\")
    ExternalProject_Add(
      tinyobjloader_dependency
      PREFIX ${TINYOBJLOADER_PREFIX}
      GIT_REPOSITORY https://github.com/syoyo/tinyobjloader.git
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:STRING=${TINYOBJLOADER_PREFIX} -DANDROID_ABI:STRING=${ANDROID_ABI} -DANDROID_NDK:STRING=${ANDROID_NDK} -DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL} -DANDROID_PLATFORM:STRING=${ANDROID_PLATFORM} -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} -DANDROID_TOOLCHAIN:STRING=${ANDROID_TOOLCHAIN} -DANDROID_STL:STRING=${ANDROID_STL} -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER} -DCMAKE_MAKE_PROGRAM:STRING=${CMAKE_MAKE_PROGRAM} -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} # -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} #-DCMAKE_DEBUG_POSTFIX:STRING=${CMAKE_DEBUG_POSTFIX}
      UPDATE_COMMAND "" #disable updating git repo every build
      LOG_DOWNLOAD ON
    )
    ExternalProject_Get_Property(tinyobjloader_dependency install_dir)
    SET(TINYOBJLOADER_INCLUDE_DIRS ${install_dir}/include)
    SET(TINYOBJLOADER_LIBRARY_PATH ${install_dir}/lib/${LIB_PREFIX}tinyobjloader${LIB_SUFFIX}${LIB_EXTENSION})
    ADD_LIBRARY(tinyobjloaderLibrary STATIC IMPORTED)
    SET_PROPERTY(TARGET tinyobjloaderLibrary PROPERTY IMPORTED_LOCATION ${TINYOBJLOADER_LIBRARY_PATH})
    SET(TINYOBJLOADER_LIBRARY tinyobjloaderLibrary)
    
    ADD_CUSTOM_COMMAND(
      OUTPUT
        ${TINYOBJLOADER_LIBRARY_PATH} 
        ${install_dir}/include/tiny_obj_loader.h 
        ${TINYOBJLOADER_PREFIX}/src/tinyobjloader_dependency-stamp/tinyobjloader_dependency-gitinfo.txt 
        ${TINYOBJLOADER_PREFIX}/tmp/tinyobjloader_dependency-cfgcmd.txt
      COMMAND
    )
    ADD_CUSTOM_TARGET(tinyobjloaderProvider DEPENDS
        ${TINYOBJLOADER_LIBRARY_PATH} 
        ${install_dir}/include/tiny_obj_loader.h 
        ${TINYOBJLOADER_PREFIX}/src/tinyobjloader_dependency-stamp/tinyobjloader_dependency-gitinfo.txt 
        ${TINYOBJLOADER_PREFIX}/tmp/tinyobjloader_dependency-cfgcmd.txt
    )
#ELSE(NOT USE_EXISTING_TINYOBJLOADER)
    #FIND_PACKAGE(tinyobjloader REQUIRED)
ENDIF(NOT USE_EXISTING_TINYOBJLOADER)

IF(NOT USE_EXISTING_BULLET AND NOT BULLET_FOUND)
    FIND_PACKAGE(Git REQUIRED)
	MESSAGE(" Bullet not found, downloading via GIT")
	SET(BULLET_PREFIX ${CMAKE_BINARY_DIR}/bullet)
	INCLUDE(ExternalProject)
	ExternalProject_Add(
	  bullet_dependency
	  CMAKE_GENERATOR "${CMAKE_GENERATOR}"
	  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:STRING=${BULLET_PREFIX} -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER} -DCMAKE_MAKE_PROGRAM:STRING=${CMAKE_MAKE_PROGRAM} -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} -DBUILD_EXTRAS:BOOL=OFF -DINSTALL_LIBS:BOOL=ON -DUSE_GLUT:BOOL=OFF -DBUILD_UNIT_TESTS:BOOL=OFF -DBUILD_CPU_DEMOS:BOOL=OFF -DBUILD_BULLET3:BOOL=OFF -DBUILD_BULLET2_DEMOS:BOOL=OFF -DBUILD_OPENGL3_DEMOS:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DANDROID_ABI:STRING=${ANDROID_ABI} -DANDROID_NDK:STRING=${ANDROID_NDK} -DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL} -DANDROID_PLATFORM:STRING=${ANDROID_PLATFORM} -DANDROID_TOOLCHAIN:STRING=${ANDROID_TOOLCHAIN} -DANDROID_STL:STRING=${ANDROID_STL}
	  PREFIX ${BULLET_PREFIX}
	  GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
	  UPDATE_COMMAND "" #disable updating git repo every build
	  LOG_DOWNLOAD ON
	)

    ExternalProject_Get_Property(bullet_dependency install_dir)
    SET(BULLET_INCLUDE_DIRS ${install_dir}/include ${install_dir}/include/bullet)
    SET(BULLET_LINEARMATH_LIBRARY_PATH ${install_dir}/lib/${LIB_PREFIX}LinearMath${LIB_SUFFIX}${LIB_EXTENSION})
    SET(BULLET_COLLISION_LIBRARY_PATH ${install_dir}/lib/${LIB_PREFIX}BulletCollision${LIB_SUFFIX}${LIB_EXTENSION})
    SET(BULLET_DYNAMICS_LIBRARY_PATH ${install_dir}/lib/${LIB_PREFIX}BulletDynamics${LIB_SUFFIX}${LIB_EXTENSION})

	ADD_LIBRARY(LinearMathLibrary STATIC IMPORTED)
	#ADD_LIBRARY(BulletInverseDynamicsLibrary STATIC IMPORTED)
	ADD_LIBRARY(BulletCollisionLibrary STATIC IMPORTED)
	ADD_LIBRARY(BulletDynamicsLibrary STATIC IMPORTED)
	#ADD_LIBRARY(BulletSoftBodyLibrary STATIC IMPORTED)

	SET_PROPERTY(TARGET LinearMathLibrary PROPERTY IMPORTED_LOCATION ${BULLET_PREFIX}/lib/${LIB_PREFIX}LinearMath${LIB_SUFFIX}${LIB_EXTENSION})
	#SET_PROPERTY(TARGET BulletInverseDynamicsLibrary PROPERTY IMPORTED_LOCATION ${BULLET_PREFIX}/lib/${LIB_PREFIX}BulletInverseDynamics${LIB_SUFFIX}${LIB_EXTENSION})
	SET_PROPERTY(TARGET BulletCollisionLibrary PROPERTY IMPORTED_LOCATION ${BULLET_PREFIX}/lib/${LIB_PREFIX}BulletCollision${LIB_SUFFIX}${LIB_EXTENSION})
	SET_PROPERTY(TARGET BulletDynamicsLibrary PROPERTY IMPORTED_LOCATION ${BULLET_PREFIX}/lib/${LIB_PREFIX}BulletDynamics${LIB_SUFFIX}${LIB_EXTENSION})
	#SET_PROPERTY(TARGET BulletSoftBodyLibrary PROPERTY IMPORTED_LOCATION ${BULLET_PREFIX}/lib/${LIB_PREFIX}BulletSoftBody${LIB_SUFFIX}${LIB_EXTENSION})

	SET(BULLET_LIBRARIES BulletDynamicsLibrary BulletCollisionLibrary LinearMathLibrary) #BulletInverseDynamicsLibrary

     SET(BULLET_LIBRARY_PATHS ${BULLET_LINEARMATH_LIBRARY_PATH} ${BULLET_COLLISION_LIBRARY_PATH} ${BULLET_DYNAMICS_LIBRARY_PATH})
	 ADD_CUSTOM_COMMAND(
      OUTPUT
        ${BULLET_LIBRARY_PATHS}
        #${install_dir}/include/tiny_obj_loader.h
        ${BULLET_PREFIX}/src/bullet_dependency-stamp/bullet_dependency-gitinfo.txt
        ${BULLET_PREFIX}/tmp/bullet_dependency-cfgcmd.txt
      COMMAND
    )
    ADD_CUSTOM_TARGET(bulletProvider DEPENDS
        ${BULLET_LIBRARY_PATHS}
        #${install_dir}/include/tiny_obj_loader.h
        ${BULLET_PREFIX}/src/bullet_dependency-stamp/bullet_dependency-gitinfo.txt
        ${BULLET_PREFIX}/tmp/bullet_dependency-cfgcmd.txt
    )

ELSE(USE_EXISTING_BULLET)
	FIND_PACKAGE(Bullet REQUIRED)
ENDIF(NOT USE_EXISTING_BULLET AND NOT BULLET_FOUND)

SET(SRC_LOCATION ${CMAKE_SOURCE_DIR}/../../../../native)

FILE(GLOB GRAPHICS_LIBRARY_SOURCE_FILES
    "${SRC_LOCATION}/include/graphics/*.h"
    "${SRC_LOCATION}/src/graphics/*.cc"
    "${SRC_LOCATION}/src/graphics/*.c"
)

FILE(GLOB APPLICATION_LIBRARY_SOURCE_FILES
    "${SRC_LOCATION}/include/application/*.h"
    "${SRC_LOCATION}/src/application/*.cc"
    "${SRC_LOCATION}/include/physics/*.h"
    "${SRC_LOCATION}/src/physics/*.cc"
)

include_directories(${SRC_LOCATION}/include/)
include_directories(${SRC_LOCATION}/include/common/)
include_directories(${SRC_LOCATION}/include/graphics/)
include_directories(${SRC_LOCATION}/include/application/)
include_directories(${SRC_LOCATION}/include/android/)
include_directories(${SRC_LOCATION}/include/physics/)

INCLUDE_DIRECTORIES(${GLM_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${TINYOBJLOADER_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${BULLET_INCLUDE_DIRS})

if(${ANDROID_PLATFORM_LEVEL} LESS 11)
  message(ERROR "OpenGL 2 is not supported before API level 11 (currently using ${ANDROID_PLATFORM_LEVEL}).")
  return()
ELSEIF(${ANDROID_PLATFORM_LEVEL} LESS 18)
  MESSAGE("Building with GLES 2")
  add_definitions("-DDYNAMIC_ES3")
  set(OPENGL_LIB GLESv2)
ELSE(${ANDROID_PLATFORM_LEVEL} LESS 11)
  MESSAGE("Building with GLES 3")
  set(OPENGL_LIB GLESv3)
ENDIF(${ANDROID_PLATFORM_LEVEL} LESS 11)

add_library(glappjni SHARED
  ${SRC_LOCATION}/src/android/gl3stub_wrapper.c
  ${SRC_LOCATION}/src/android/glappjni.cc
  ${GRAPHICS_LIBRARY_SOURCE_FILES}
  ${APPLICATION_LIBRARY_SOURCE_FILES}
)

SET_TARGET_PROPERTIES(glappjni PROPERTIES LINKER_LANGUAGE CXX)

#IF(NOT USE_EXISTING_GLM)
#	ADD_DEPENDENCIES(glappjni glm_dependency)
#ENDIF(NOT USE_EXISTING_GLM)

#IF(NOT USE_EXISTING_TINYOBJLOADER)
#	ADD_DEPENDENCIES(glappjni tinyobjloader_dependency)
#ENDIF(NOT USE_EXISTING_TINYOBJLOADER)

ADD_DEPENDENCIES(glappjni glm_dependency tinyobjloader_dependency bullet_dependency glmProvider tinyobjloaderProvider bulletProvider)

# add lib dependencies
target_link_libraries(glappjni
                      ${OPENGL_LIB}
                      android
                      EGL
                      log
                      m
                      ${TINYOBJLOADER_LIBRARY}
                      ${BULLET_LIBRARIES}
                      ${CMAKE_DL_LIBS}
)
