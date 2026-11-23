// Copyright (C) 2017-2026 Chris Liebert

#ifndef _CAMERA_H_
#define _CAMERA_H_

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>

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

	void aim(double dHeading, double dPitch);
	void moveForward(double distance);
	void moveBackward(double distance);
	void moveLeft(double distance);
	void moveRight(double distance);
	void update();
};

#endif // _CAMERA_H_
