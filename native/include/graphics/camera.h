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
	float fov_degrees;
	float aspect_ratio;
	float near_plane;
	float far_plane;

	void aim(double dHeading, double dPitch);
	void moveForward(double distance);
	void moveBackward(double distance);
	void moveLeft(double distance);
	void moveRight(double distance);
	void setFov(float fov_deg);
	void setProjection(float fov_deg, float aspect, float near_p = 0.1f, float far_p = 1000.0f);
	void updateProjection();
	void update();
};

#endif // _CAMERA_H_
