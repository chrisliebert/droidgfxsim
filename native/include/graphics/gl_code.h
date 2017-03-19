// Copyright (C) 2017 Chris Liebert

#ifndef _GL2_CODE_H_
#define _GL2_CODE_H_

#include "common/log.h"

#ifndef DESKTOP_APP
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
} Image;

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource);
Image* loadImage(const char* filename);
GLuint loadTexture(Image* texture);

#define BUFFER_OFFSET(x)((char *)NULL+(x))

// Android GLES 2.0-compatible VBO (NO-VAO)
#define DECLARE_EXPORTED_BLENDER_MODEL_GL2_CLASS()													\
	class Model: public renderable::Static {																					\
	private:																						\
		unsigned int vbo[OBJECTS_COUNT];															\
		unsigned int vinx[OBJECTS_COUNT];															\
	public:																							\
		Model() {																					\
			unsigned int i;																			\
			glGenBuffers(OBJECTS_COUNT, vbo);														\
			for (i = 0; i < OBJECTS_COUNT; i++) {													\
				glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);												\
				glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex_struct) * vertex_count[i],		\
							 &vertices[vertex_offset_table[i]], GL_STATIC_DRAW);					\
				glBindBuffer(GL_ARRAY_BUFFER, 0);													\
			}																						\
			glGenBuffers(OBJECTS_COUNT, vinx);														\
			for (i = 0; i < OBJECTS_COUNT; i++) {													\
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[i]);										\
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes[0]) * faces_count[i] * 3,		\
							 &indexes[indices_offset_table[i]], GL_STATIC_DRAW);					\
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);											\
			}																						\
		}																							\
		virtual ~Model() {																			\
			unsigned int i;																			\
			for(i = 0; i< OBJECTS_COUNT; i++) {														\
				glDeleteBuffers(1, &vbo[i]);														\
				glDeleteBuffers(1, &vinx[i]);														\
			}																						\
		}																							\
		void drawMesh(unsigned int index) {															\
			/* TODO: incorporate transformations */													\
			glBindBuffer(GL_ARRAY_BUFFER, vbo[index]);												\
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[index]);										\
			glEnableVertexAttribArray(0);															\
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex_struct),			\
				BUFFER_OFFSET(0));																	\
			glEnableVertexAttribArray(1);															\
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex_struct),			\
				BUFFER_OFFSET(3 * sizeof(float)));													\
			glEnableVertexAttribArray(2);															\
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex_struct),			\
				BUFFER_OFFSET(6 * sizeof(float)));													\
			glDrawElements(GL_TRIANGLES, faces_count[index] * 3, INX_TYPE, BUFFER_OFFSET(0));		\
			glDisableVertexAttribArray(2);															\
			glDisableVertexAttribArray(1);															\
			glDisableVertexAttribArray(0);															\
			glBindBuffer(GL_ARRAY_BUFFER, 0);														\
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);												\
		}																							\
		void render()																				\
		{																							\
			unsigned int i;																			\
			for (i=0; i<OBJECTS_COUNT; i++) {														\
				drawMesh(i);																		\
			}																						\
		}																							\
	};

// Android GLES 3.0-compatible using VAO
#define DECLARE_EXPORTED_BLENDER_MODEL_GL3_CLASS()													\
	class Model: public renderable::Static {																					\
	private:																						\
		unsigned int vbo[OBJECTS_COUNT];															\
		unsigned int vinx[OBJECTS_COUNT];															\
	public:																							\
		Model() {																					\
			unsigned int i;																			\
			glGenBuffers(OBJECTS_COUNT, vbo);														\
			for (i = 0; i < OBJECTS_COUNT; i++) {													\
				glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);												\
				glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex_struct) * vertex_count[i],		\
							 &vertices[vertex_offset_table[i]], GL_STATIC_DRAW);					\
				glBindBuffer(GL_ARRAY_BUFFER, 0);													\
			}																						\
			glGenBuffers(OBJECTS_COUNT, vinx);														\
			for (i = 0; i < OBJECTS_COUNT; i++) {													\
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[i]);										\
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes[0]) * faces_count[i] * 3,		\
							 &indexes[indices_offset_table[i]], GL_STATIC_DRAW);					\
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);											\
			}																						\
		}																							\
		virtual ~Model() {																			\
			unsigned int i;																			\
			for(i = 0; i< OBJECTS_COUNT; i++) {														\
				glDeleteBuffers(1, &vbo[i]);														\
				glDeleteBuffers(1, &vinx[i]);														\
			}																						\
		}																							\
		void drawMesh(unsigned int index) {															\
			/* TODO: incorporate transformations */													\
			glBindBuffer(GL_ARRAY_BUFFER, vbo[index]);												\
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[index]);										\
			glEnableClientState(GL_VERTEX_ARRAY);													\
			glVertexPointer(3, GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(0));			\
			glEnableClientState(GL_NORMAL_ARRAY);													\
			glNormalPointer(GL_FLOAT, sizeof (struct vertex_struct),								\
				BUFFER_OFFSET(3 * sizeof (float)));													\
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);											\
			glTexCoordPointer(2, GL_FLOAT, sizeof (struct vertex_struct),							\
			BUFFER_OFFSET(6 * sizeof (float)));														\
			glDrawElements(GL_TRIANGLES, faces_count[index] * 3, INX_TYPE, BUFFER_OFFSET(0));		\
			glBindBuffer(GL_ARRAY_BUFFER, 0);														\
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);												\
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);											\
			glDisableClientState(GL_NORMAL_ARRAY);													\
			glDisableClientState(GL_VERTEX_ARRAY);													\
		}																							\
		void render()																				\
		{																							\
			unsigned int i;																			\
			for (i=0; i<OBJECTS_COUNT; i++) {														\
				drawMesh(i);																		\
			}																						\
		}																							\
	};

#endif //_GL_CODE_H_
