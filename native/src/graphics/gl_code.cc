// Copyright (C) 2017 Chris Liebert

#include "graphics/gl_code.h"

#define STB_IMAGE_IMPLEMENTATION
#include "graphics/stb_image.h"

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

#ifdef DESKTOP_APP
Image* loadImage(const char* filename) {
	Image* image = new Image();
	assert(image);
	image->data = stbi_load(filename, &image->w, &image->h, &image->comp, STBI_default);
	if(image->data == 0) {
		LOGE("Unable to load image: %s", filename);
		return 0;
	}
	return image;
}
#endif

GLuint loadTexture(Image* texture) {
	assert(texture);
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (texture->comp == 3) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture->w, texture->h, 0, GL_RGB, GL_UNSIGNED_BYTE, texture->data);
	}
	else if (texture->comp == 4) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->w, texture->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->data);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(texture->data);
    texture->data = 0;
	return texture_id;
}

#define MAX_SHADER_INFO_LOG_LENGTH 4096
GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        checkGlError("glCompileShader");
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 0) {
				if (infoLen >= MAX_SHADER_INFO_LOG_LENGTH) { LOGE("Warning, shader info log truncated"); }
                char buf[MAX_SHADER_INFO_LOG_LENGTH];
                glGetShaderInfoLog(shader, infoLen, NULL, &buf[0]);
                LOGE("Could not compile shader: %d:\n%s\n",
                        shaderType, buf);
                glDeleteShader(shader);
                shader = 0;
                exit(10);
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength > 0) {
				if (bufLength >= MAX_SHADER_INFO_LOG_LENGTH) { LOGE("Warning, shader program info log truncated"); }
				char buf[MAX_SHADER_INFO_LOG_LENGTH];
                glGetProgramInfoLog(program, bufLength, NULL, &buf[0]);
                LOGE("Could not link program:\n%s\n", &buf[0]);
            }
            glDeleteProgram(program);
            program = 0;
        } else {
        	// Delete the shader objects once the program is linked
        	//glDeleteShader(vertexShader);
        	//glDeleteShader(pixelShader);
        }
    } else {
    	LOGE("Unable to create program\n");
    }
    return program;
}

