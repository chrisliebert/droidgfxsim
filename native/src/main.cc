﻿// Copyright (C) 2017 Chris Liebert

#include <glad/glad.h>
#include <GLFW/glfw3.h>
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
// (this is necessary to ensure render function is called when debugging)
// A high of a value will reduce input responsiveness
#ifndef MAX_SEQUENTIAL_UPDATE_COUNT
#define MAX_SEQUENTIAL_UPDATE_COUNT 60
#endif

#ifndef RENDERER
#define RENDERER GL2SceneGraphRenderer
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
				delete application;
				application = 0;
			}
			if (key == GLFW_KEY_SPACE) {
				//TransformNode* t = (TransformNode*) application->scenegraph_root->find("Cube.001");

				//btVector3 origin(0.0, 10.0, 0.0);
				//btTransform transform;
				//transform.setOrigin(origin);
				//application->physics_nodes[1].body->setWorldTransform(transform);
				btVector3 up_force(0.0, 100.0, 0.0);
				const btVector3 rel_pos(0.0, 1.0, 0.0);
				application->simulation->physics_nodes[1].body->applyForce(
						up_force, rel_pos);
				application->simulation->physics_nodes[2].body->applyForce(
						up_force, rel_pos);
				application->simulation->physics_nodes[3].body->applyForce(
						up_force, rel_pos);
				//btVector3 angular_velocity(1.0, 1.0, 1.0);
				//application->physics_nodes[1].body->setAngularVelocity(angular_velocity);

				//glm::translate(t->matrix, glm::vec3(0.0, 10.0, 0.0));
			}

			// Close window
			if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
				glfwSetWindowShouldClose(window, GL_TRUE);
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
	//todo: create new renderer on resize or allow resize function in renderer
	if (application)
		application->resize(fb_w, fb_h);
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
		double currentTime = glfwGetTime();
		double accumulator = 0.0;

		RENDERER* renderer = 0;
		while (glfwWindowShouldClose(window) == GL_FALSE) {
			glfwPollEvents();
			if (!application) {
				if (renderer)
					delete renderer;
				application = new Application();
				assert(application);
				renderer = new RENDERER(application->images);
				assert(renderer);
				reshapeFunc(window, width, height);
			}
			double newTime = glfwGetTime();
			double frameTime = newTime - currentTime;
			currentTime = newTime;
			accumulator += frameTime;
			unsigned sequential_update_count = 0;
			while (accumulator >= dt) {
				application->step();
				accumulator -= dt;
				t += dt;
				sequential_update_count++;
				if (sequential_update_count > MAX_SEQUENTIAL_UPDATE_COUNT) {
					application->render(renderer);
					glfwSwapBuffers(window);
				}
			}
			application->render(renderer);
			glfwSwapBuffers(window);
		}
		// Hide window before freeing objects to improve responsiveness
		glfwHideWindow(window);
		delete renderer;
	}

	if (application)
		delete application;

	glfwDestroyWindow(window);

	return 0;
}
