// Copyright (C) 2017 Chris Liebert

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "graphics/gl_code.h"

static void checkGlError(const char* op) {
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGE("after %s() glError (0x%x)\n", op, error);
	}
}

Image::Image(const char *filename, AssetManager *manager) {
	Image();
	if(!loadAsset(filename, manager)) {
		LOGE("Unable to load image: %s", filename);
	}
}

Image::Image() {
	w = 0;
	h = 0;
	comp = 0;
	data = 0;
}

Image::~Image() {
	if(data) {
		STBI_FREE(data);		
	}
}

bool Image::loadAsset(const char* filename, AssetManager* manager) {
    size_t file_length = 0;
    unsigned char *image_file_bytes = manager->loadBinaryFile(filename, file_length);
    if(file_length == 0) { return false; }
    data = stbi_load_from_memory(image_file_bytes, (GLsizei)(file_length * sizeof(unsigned char)),
                                        &w, &h, &comp, STBI_default);
    delete [] image_file_bytes;
    if (data == 0) {
        return false;
    }
    return true;
}

GLuint Image::loadTexture() {
	if(0 == data) {
		// Generate blank white texture if there was a problem loading the texture
		w = 8;
		h = 8;
		comp = 3;
		size_t num_pixels = comp * w * h;
		data = (unsigned char*) STBI_MALLOC(num_pixels);
		for(size_t i=0; i<num_pixels; i++) {
			data[i] = 255;
		}
	}
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (comp == 3) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
				GL_RGB, GL_UNSIGNED_BYTE, data);
	} else if (comp == 4) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, data);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	return texture_id;
}

#define MAX_SHADER_INFO_LOG_LENGTH 4096
GLuint loadShader(GLenum shader_type, const char* shader_source) {
	GLuint shader = glCreateShader(shader_type);
	if (shader) {
		glShaderSource(shader, 1, &shader_source, NULL);
		glCompileShader(shader);
		checkGlError("glCompileShader");
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			GLint infoLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
			if (infoLen > 0) {
				if (infoLen >= MAX_SHADER_INFO_LOG_LENGTH) {
					LOGE("Warning, shader info log truncated");
				}
				char buf[MAX_SHADER_INFO_LOG_LENGTH];
				glGetShaderInfoLog(shader, infoLen, NULL, &buf[0]);
				LOGE("Could not compile shader: %d:\n%s\n", shader_type, buf);
				glDeleteShader(shader);
				shader = 0;
				exit(10);
			}
		}
	}
	return shader;
}

GLuint createProgram(const char* vertex_source, const char* fragment_source) {
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertex_source);
	if (!vertexShader) {
		return 0;
	}

	GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, fragment_source);
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
			GLint buflen = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buflen);
			if (buflen > 0) {
				if (buflen >= MAX_SHADER_INFO_LOG_LENGTH) {
					LOGE("Warning, shader program info log truncated");
				}
				char buf[MAX_SHADER_INFO_LOG_LENGTH];
				glGetProgramInfoLog(program, buflen, NULL, &buf[0]);
				LOGE("Could not link program:\n%s\n", &buf[0]);
			}
			glDeleteProgram(program);
			program = 0;
		} else {
			// Delete the shader objects once the program is linked
			glDeleteShader(vertexShader);
			glDeleteShader(pixelShader);
		}
	} else {
		LOGE("Unable to create program\n");
	}
	return program;
}

