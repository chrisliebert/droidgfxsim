// Copyright (C) 2017-2026 Chris Liebert

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include <iostream>

#define DESKTOP_APP 1
#include "application/application.h"
#include "graphics/gl_code.h"
#include "graphics/gl2_renderer.h"
#include "graphics/gl3_renderer.h"
#include "graphics/particle_system.h"
#include "graphics/imgui_overlay.h"
#include "physics/raycast_picker.h"
#include "physics/projectile_spawner.h"
#include <imgui.h>

#if defined(DROIDGFXSIM_HAS_VULKAN) && defined(__has_include)
#if __has_include("graphics/vulkan_renderer.h")
#include "graphics/vulkan_renderer.h"
#define DROIDGFXSIM_ENABLE_VULKAN 1
#endif
#endif

#ifndef MAX_SEQUENTIAL_UPDATE_COUNT
#define MAX_SEQUENTIAL_UPDATE_COUNT 60
#endif

int width = 1200;
int height = 800;
Application* application = nullptr;
bool is_mouse_dragging = false;
double last_mouse_x = 0.0;
double last_mouse_y = 0.0;
bool headless = false;
ImGuiOverlay imgui_overlay;
ParticleSystem particle_system;

void clickFunc(GLFWwindow* window, int button, int action, int mods) {
	(void) mods;
	if (!headless && ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT || (button == GLFW_MOUSE_BUTTON_LEFT && (mods & GLFW_MOD_ALT))) {
		if (action == GLFW_PRESS) {
			double mx, my;
			glfwGetCursorPos(window, &mx, &my);
			if (application) {
				bool picked = application->pickBody(static_cast<float>(mx), static_cast<float>(my), width, height);
				if (picked && application->picker && application->picker->getPickedBody()) {
					btTransform trans = application->picker->getPickedBody()->getWorldTransform();
					btVector3 pivot = trans(application->picker->getLocalPivot());
					particle_system.emitBurst(
						glm::vec3(pivot.x(), pivot.y(), pivot.z()),
						glm::vec3(0.0f, 1.0f, 0.0f),
						20,
						glm::vec4(0.3f, 0.8f, 1.0f, 1.0f)
					);
				}
			}
		} else if (action == GLFW_RELEASE) {
			if (application) {
				application->releaseBody();
			}
		}
	} else if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			is_mouse_dragging = true;
			glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);
		} else if (action == GLFW_RELEASE) {
			is_mouse_dragging = false;
		}
	}
}

void keyboardFunc(GLFWwindow* window, int key, int scancode, int action, int mods) {
	(void) window; (void) scancode; (void) mods;
	if (!headless && ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard) {
		if (key != GLFW_KEY_ESCAPE && key != GLFW_KEY_Q) {
			return;
		}
	}

	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			return;
		}

		if (application) {
			if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
				application->shootProjectile(35.0f);
				if (application->camera) {
					particle_system.emitBurst(
						application->camera->position + application->camera->direction * 2.0f,
						application->camera->direction,
						25,
						glm::vec4(1.0f, 0.6f, 0.2f, 1.0f)
					);
				}
			}
			if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
				application->toggleDebugDrawer();
			}
			if (key == GLFW_KEY_C && action == GLFW_PRESS) {
				application->cascade_mode_active = !application->cascade_mode_active;
				LOGI("Continuous Cascade Mode %s", application->cascade_mode_active ? "ENABLED" : "DISABLED");
			}
			if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
				application->loadScenePreset(0);
			}
			if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
				application->loadScenePreset(1);
			}
			if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
				application->loadScenePreset(2);
			}
			if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
				application->loadScenePreset(3);
			}
			if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
				delete application;
				application = nullptr;
			}
			if (key == GLFW_KEY_ENTER) {
				delete application;
				application = nullptr;
			}

			if (application && application->camera) {
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
				if (key == GLFW_KEY_A) {
					application->camera->moveLeft(1.0);
					application->camera->update();
				}
				if (key == GLFW_KEY_D) {
					application->camera->moveRight(1.0);
					application->camera->update();
				}
				if (key == GLFW_KEY_W) {
					application->camera->position.y += 1.0f;
					application->camera->update();
				}
				if (key == GLFW_KEY_S) {
					application->camera->position.y -= 1.0f;
					application->camera->update();
				}
			}
		}
	}
}

void motionFunc(GLFWwindow* window, double mouse_x, double mouse_y) {
	(void) window;
	if (!headless && ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
		return;
	}

	if (application && application->isPicking()) {
		application->dragBody(static_cast<float>(mouse_x), static_cast<float>(mouse_y), width, height);
		if (application->picker && application->picker->getPickedBody()) {
			btTransform trans = application->picker->getPickedBody()->getWorldTransform();
			btVector3 pivot = trans(application->picker->getLocalPivot());
			particle_system.emitBurst(
				glm::vec3(pivot.x(), pivot.y(), pivot.z()),
				glm::vec3(0.0f, 1.0f, 0.0f),
				4,
				glm::vec4(0.2f, 0.7f, 1.0f, 0.9f)
			);
		}
	} else if (is_mouse_dragging) {
		double dx = mouse_x - last_mouse_x;
		double dy = mouse_y - last_mouse_y;
		if (application && application->camera) {
			const double sensitivity = 0.005;
			application->camera->aim(-dx * sensitivity, -dy * sensitivity);
			application->camera->update();
		}
	}
	last_mouse_x = mouse_x;
	last_mouse_y = mouse_y;
}

void reshapeFunc(GLFWwindow* window, int w, int h) {
	int fb_w, fb_h;
	glfwGetFramebufferSize(window, &fb_w, &fb_h);
	glViewport(0, 0, fb_w, fb_h);
	if (application) {
		application->resize(fb_w, fb_h);
	}
	width = w;
	height = h;
}

int main(int argc, char** argv) {
	bool force_gl2 = false;
	bool force_gl3 = false;
	bool force_vulkan = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--gl2") == 0) force_gl2 = true;
		if (strcmp(argv[i], "--gl3") == 0) force_gl3 = true;
		if (strcmp(argv[i], "--vulkan") == 0) force_vulkan = true;
		if (strcmp(argv[i], "--headless") == 0) headless = true;
	}

	if (!glfwInit()) {
		LOGE("Failed to initialize GLFW.");
		return -1;
	}

	if (force_gl3) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	}

	if (headless) {
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	}

	GLFWwindow* window = glfwCreateWindow(width, height, "Simulator", NULL, NULL);
	if (window == NULL) {
		LOGE("Failed to open GLFW window.");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		LOGE("Failed to initialize GLAD!");
	}

	glfwSwapInterval(0);
	glfwSetWindowSizeCallback(window, reshapeFunc);
	glfwSetKeyCallback(window, keyboardFunc);
	glfwSetMouseButtonCallback(window, clickFunc);
	glfwSetCursorPosCallback(window, motionFunc);

	LOGI("OpenGL %i.%i initialized", GLVersion.major, GLVersion.minor);

	particle_system.init(2000);

	if (!headless) {
		imgui_overlay.init(window);
	}

	{
		const double dt = 0.0175;
		double current_time = glfwGetTime();
		double accumulator = 0.0;

		IRenderer* renderer = nullptr;

		while (glfwWindowShouldClose(window) == GLFW_FALSE) {
			glfwPollEvents();
			double new_time = glfwGetTime();
			double frame_time = new_time - current_time;
			current_time = new_time;
			accumulator += frame_time;
			unsigned sequential_update_count = 0;

			while (accumulator >= dt) {
				glfwPollEvents();
				if (glfwWindowShouldClose(window) == GLFW_TRUE) {
					break;
				}

				if (!application) {
					if (renderer) {
						delete renderer;
						renderer = nullptr;
					}
					application = new Application();
					assert(application);

					if (force_vulkan) {
#if defined(DROIDGFXSIM_ENABLE_VULKAN)
						try {
							LOGI("Attempting Vulkan renderer initialization...");
							auto* vk_renderer = new VulkanRenderer();
							if (vk_renderer->init(window)) {
								renderer = vk_renderer;
							} else {
								delete vk_renderer;
								renderer = nullptr;
								LOGE("VulkanRenderer initialization failed, falling back to OpenGL");
							}
						} catch (...) {
							LOGE("VulkanRenderer exception caught, falling back to OpenGL");
							renderer = nullptr;
						}
#else
						LOGW("--vulkan requested, but Vulkan backend is not built into this configuration. Falling back to OpenGL.");
#endif
					}

					if (!renderer) {
						if (force_gl2 || GLVersion.major < 3) {
							renderer = new GL2SceneGraphRenderer(application->images);
						} else {
							renderer = new GL3SceneGraphRenderer(application->images);
						}
					}
					assert(renderer);
					reshapeFunc(window, width, height);
					current_time = glfwGetTime();
					accumulator = 0.0;
				}

				application->step();
				particle_system.update(static_cast<float>(dt));

				// Detect dynamic physics impacts for contact sparks
				if (application->simulation && application->simulation->dynamics_world) {
					auto* dispatcher = application->simulation->dynamics_world->getDispatcher();
					if (dispatcher) {
						int numManifolds = dispatcher->getNumManifolds();
						for (int m = 0; m < numManifolds; m++) {
							btPersistentManifold* contactManifold = dispatcher->getManifoldByIndexInternal(m);
							int numContacts = contactManifold->getNumContacts();
							for (int c = 0; c < numContacts; c++) {
								const btManifoldPoint& pt = contactManifold->getContactPoint(c);
								if (pt.getAppliedImpulse() > 6.0f) {
									btVector3 cp = pt.getPositionWorldOnA();
									btVector3 norm = pt.m_normalWorldOnB;
									int spark_count = std::clamp(static_cast<int>(pt.getAppliedImpulse() * 0.5f), 5, 35);
									particle_system.emitBurst(
										glm::vec3(cp.x(), cp.y(), cp.z()),
										glm::vec3(norm.x(), norm.y(), norm.z()),
										spark_count,
										glm::vec4(1.0f, 0.65f, 0.2f, 1.0f)
									);
								}
							}
						}
					}
				}

				accumulator -= dt;
				sequential_update_count++;

				if (sequential_update_count > MAX_SEQUENTIAL_UPDATE_COUNT) {
					application->render(renderer);
					application->renderDebug();
					if (application->camera) {
						glm::mat4 vp = application->camera->projection_matrix * application->camera->modelview_matrix;
						particle_system.render(vp);
					}
					glfwSwapBuffers(window);
				}
			}

			if (application && renderer) {
				application->render(renderer);
				application->renderDebug();
				if (application->camera) {
					glm::mat4 vp = application->camera->projection_matrix * application->camera->modelview_matrix;
					particle_system.render(vp);
				}

				if (!headless) {
					int fps = (frame_time > 0.0001) ? static_cast<int>(1.0 / frame_time) : 60;
					TelemetryData data;
					data.fps = fps;
					data.frame_time_ms = static_cast<float>(frame_time * 1000.0);
					data.rigid_body_count = application->getRigidBodyCount();
					data.rendered_count = application->getRigidBodyCount();

					imgui_overlay.beginFrame();
					imgui_overlay.renderHUD(data);

					// Particle System Telemetry & Interactive Controls
					static int ui_burst_count = 40;
					ImGui::Begin("Particle FX & Telemetry", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
					ImGui::Text("Active Particles: %zu / %zu", particle_system.getActiveCount(), particle_system.getMaxCapacity());
					float frac = static_cast<float>(particle_system.getActiveCount()) / static_cast<float>(particle_system.getMaxCapacity());
					ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f));
					ImGui::SliderInt("Emission Count", &ui_burst_count, 5, 200);
					if (ImGui::Button("Trigger Spark Burst")) {
						if (application && application->camera) {
							particle_system.emitBurst(
								application->camera->position + application->camera->direction * 5.0f,
								glm::vec3(0.0f, 1.0f, 0.0f),
								ui_burst_count,
								glm::vec4(1.0f, 0.6f, 0.1f, 1.0f)
							);
						} else {
							particle_system.emitBurst(
								glm::vec3(0.0f, 2.0f, 0.0f),
								glm::vec3(0.0f, 1.0f, 0.0f),
								ui_burst_count,
								glm::vec4(1.0f, 0.6f, 0.1f, 1.0f)
							);
						}
					}
					ImGui::SameLine();
					if (ImGui::Button("Clear All")) {
						particle_system.clear();
					}
					ImGui::End();

					// Aerial Cascade Spawner & Telemetry
					ImGui::Begin("Cascade Simulation", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
					ImGui::Checkbox("Continuous Cascade (Key 'C')", &application->cascade_mode_active);
					ImGui::SliderFloat("Cascade Interval (s)", &application->cascade_interval, 0.05f, 2.0f, "%.2f s");
					if (ImGui::Button("Spawn Wave (10 Bodies)")) {
						for (int i = 0; i < 10; ++i) {
							float rx = -9.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 18.0f));
							float ry = 28.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 14.0f));
							float rz = -12.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 5.0f));
							float rrad = 0.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.6f));
							float rmass = 1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 2.0f));
							application->spawnDynamicDebris(rx, ry, rz, rrad, rmass);
						}
					}
					ImGui::Text("Active Bodies: %d", application->getRigidBodyCount());
					ImGui::End();

					auto& controls = imgui_overlay.getControls();
					if (controls.step_requested) {
						controls.step_requested = false;
						application->step();
					}
					if (controls.preset_changed) {
						controls.preset_changed = false;
						application->loadScenePreset(controls.selected_preset);
					}

					imgui_overlay.endFrame();
				}

				glfwSwapBuffers(window);
			}

			if (headless && current_time > 2.0) {
				// Headless test verification exit
				break;
			}
		}

		if (!headless) {
			imgui_overlay.shutdown();
		}

		glfwHideWindow(window);
		if (renderer) {
			delete renderer;
			renderer = nullptr;
		}
	}

	if (application) {
		delete application;
		application = nullptr;
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
