// Copyright (C) 2017 Chris Liebert

#ifndef _GL2_CODE_H_
#define _GL2_CODE_H_

#include "common/asset_manager.hpp"
#include "common/log.h"

#if defined(__ANDROID__)
	#include <jni.h>
  	#if DYNAMIC_ES3
    #include "gl3stub.h"
	#else
    	#include <GLES3/gl3.h>
	#endif
#else
	#include "glad/glad.h"
#endif

#include <cstdlib>
#include <cmath>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

#include "graphics/camera.h"

typedef struct Image {
	int w, h, comp;
	unsigned char* data;
	Image(const char *filename, AssetManager *manager);
	Image();
	~Image();
	bool loadAsset(const char *filename, AssetManager *manager);
	GLuint loadTexture();
} Image;

GLuint createProgram(const char* vertex_source, const char* fragment_source);

#define BUFFER_OFFSET(x)((char *)NULL+(x))

#endif //_GL_CODE_H_
