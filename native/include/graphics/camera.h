// Copyright (C) 2017 Chris Liebert

#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>

#include "graphics/gl_code.h"

class Camera {
public:
	Camera();
	glm::mat4 modelview_matrix;
	glm::mat4 projection_matrix;
	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 right;
	glm::vec3 up;
	double horizontal_angle;
	double vertical_angle;

	void aim(double x, double y);
	void moveForward(double amount);
	void moveBackward(double amount);
	void moveLeft(double amount);
	void moveRight(double amount);
	void update();
};

#endif
