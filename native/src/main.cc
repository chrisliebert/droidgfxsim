// Copyright (C) 2017 Chris Liebert

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>

#define DESKTOP_APP 1
#include "application/application.h"
#include "graphics/gl_code.h"
#include "graphics/gl2_renderer.h"
#include "graphics/gl3_renderer.h"
#include <stdarg.h>

#ifdef GLAD_DEBUG
// logs every gl call to the console
void pre_gl_call(const char *name, void *funcptr, int len_args, ...);
#endif

// GLAD_DEBUG is only defined if the c-debug generator was used
#ifdef GLAD_DEBUG
// logs every gl call to the console
void pre_gl_call(const char *name, void *funcptr, int len_args, ...) {
	LOGI("Calling: %s(", name);
	int i,val;
	va_list vl;
	va_start(vl, len_args);
	//largest=va_arg(vl,int);
	for (i = 0; i < len_args; i++)
	{
		val=va_arg(vl,int);
		LOGI("%i", val);
		if(i + 1 < len_args) {
			LOGI(", ");
		}
		//largest=(largest>val)?largest:val;
	}
	va_end(vl);
	LOGI(")\n");
}
#endif

// The maximum number of updates that can occurs before render is called
// (reducing this is sometimes necessary to ensure render function is called when debugging)
// A high of a value will reduce input responsiveness
#ifndef MAX_SEQUENTIAL_UPDATE_COUNT

#if NDEBUG
#define MAX_SEQUENTIAL_UPDATE_COUNT 60
#else
#define MAX_SEQUENTIAL_UPDATE_COUNT 10
#endif

#endif

#ifndef RENDERER
#define RENDERER GL3SceneGraphRenderer
#endif

int width = 1200;
int height = 800;

Application* application = 0;

void clickFunc(GLFWwindow* window, int button, int action, int mods) {
	(void) window;
	(void) mods;
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			std::cout << "Left Mouse Press" << std::endl;
		} else if (action == GLFW_RELEASE) {
			std::cout << "Left Mouse Release" << std::endl;
		}
	}
}

void keyboardFunc(GLFWwindow* window, int key, int scancode, int action,
		int mods) {
	(void) window;
	(void) scancode;
	(void) mods;
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		if (application) {
			// Close window and abort simulation
			if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
				glfwSetWindowShouldClose(window, GL_TRUE);
			    return;
			}
			if (key == GLFW_KEY_UP) {
				application->camera->moveForward(1.0);
				application->camera->update();
			}
			if (key == GLFW_KEY_DOWN) {
				application->camera->moveBackward(1.0);
				application->camera->update();
			}

			if (key == GLFW_KEY_LEFT) {
				application->camera->aim(0.1, 0);
				application->camera->update();
			}

			if (key == GLFW_KEY_RIGHT) {
				application->camera->aim(-0.1, 0);
				application->camera->update();
			}

			if (key == GLFW_KEY_W) {
				application->camera->position.y += 1;
				application->camera->update();
			}
			if (key == GLFW_KEY_S) {
				application->camera->position.y -= 1;
				application->camera->update();
			}
			if (key == GLFW_KEY_ENTER) {
				if(application) { delete application; }
				application = 0;
			}
		}
	}
}

void motionFunc(GLFWwindow* window, double mouse_x, double mouse_y) {
	(void) window;
}

void reshapeFunc(GLFWwindow* window, int w, int h) {
	int fb_w, fb_h;
	// Get the frame buffer size.
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	glViewport(0, 0, fb_w, fb_h);
	if (application){
		application->resize(fb_w, fb_h);
	}
	width = w;
	height = h;
}

int main(int argc, char** argv) {
	GLFWwindow* window = NULL;

	if (!glfwInit()) {
		LOGE("Failed to initialize GLFW.");
		return -1;
	}

	window = glfwCreateWindow(width, height, "Simulator", NULL, NULL);
	if (window == NULL) {
		LOGE("Failed to open GLFW window. ");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

	// Disable V-SYNC
	glfwSwapInterval(0);

	// Callback
	glfwSetWindowSizeCallback(window, reshapeFunc);
	glfwSetKeyCallback(window, keyboardFunc);
	glfwSetMouseButtonCallback(window, clickFunc);
	glfwSetCursorPosCallback(window, motionFunc);

	if (!gladLoadGL()) {
		std::cerr << "Something went wrong initializing OpenGL!" << std::endl;
	}

#ifdef GLAD_DEBUG
	// before every OpenGL call, call pre_gl_call
	glad_set_pre_callback(pre_gl_call);

	// post callback checks for glGetError by default

	// don't use the callback for glClear
	// (glClear could be replaced with your own function)
	glad_debug_glClear = glad_glClear;
#endif

	LOGI("OpenGL %i.%i", GLVersion.major, GLVersion.minor);
	if (GLVersion.major < 2) {
		LOGE("Your system doesn't support OpenGL >= 2!");
		return -1;
	}

	{
		double t = 0.0;
		const double dt = 0.0175;
		double current_time = glfwGetTime();
		double accumulator = 0.0;

		RENDERER* renderer = 0;
		while (glfwWindowShouldClose(window) == GL_FALSE) {
			glfwPollEvents();
			double new_time = glfwGetTime();
			double frame_time = new_time - current_time;
			current_time = new_time;
			accumulator += frame_time;
			unsigned sequential_update_count = 0;
			while (accumulator >= dt) {
				// Check for events in inner loop to process input more frequently when framerate is low
								
				glfwPollEvents();
				// Check to see if the simulation has been aborted while framerate is low
				if(glfwWindowShouldClose(window) == GL_TRUE) {
				    break;
				}

				if (!application) {
					if (renderer) { delete renderer; }
					application = new Application();
					assert(application);
					renderer = new RENDERER(application->images);
					assert(renderer);
					reshapeFunc(window, width, height);
					// Reset simulation time
					t = 0.0;
					current_time = glfwGetTime();
					new_time = current_time;
					accumulator = 0.0;
				}
				
				application->step();
				accumulator -= dt;
				t += dt;
				sequential_update_count++;
				if (sequential_update_count > MAX_SEQUENTIAL_UPDATE_COUNT) {

					application->render(renderer);
					glfwSwapBuffers(window);			
				}
			}
			
			if(application) {
				application->render(renderer);
				glfwSwapBuffers(window);
			}
		}
		// Hide window before freeing objects to improve responsiveness
		glfwHideWindow(window);
		delete renderer;
	}

	if (application) {
		delete application;
	}

	glfwDestroyWindow(window);
	return 0;
}
