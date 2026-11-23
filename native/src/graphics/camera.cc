// Copyright (C) 2017-2026 Chris Liebert

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cmath>
#include "graphics/camera.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera::Camera()
	: modelview_matrix(1.0f),
	  projection_matrix(1.0f),
	  position(0.0f, 0.0f, 0.0f),
	  direction(0.0f, 0.0f, -1.0f),
	  right(1.0f, 0.0f, 0.0f),
	  up(0.0f, 1.0f, 0.0f),
	  horizontal_angle(M_PI),
	  vertical_angle(0.0) {
	aim(0.0, 0.0);
	update();
}

void Camera::aim(double dHeading, double dPitch) {
	horizontal_angle += dHeading;
	vertical_angle += dPitch;

	// Clamp pitch angle [-1.55, 1.55] (~88.8 deg) to prevent gimbal inversion
	const double maxPitch = 1.55;
	vertical_angle = std::clamp(vertical_angle, -maxPitch, maxPitch);

	direction = glm::vec3(
		cos(vertical_angle) * sin(horizontal_angle),
		sin(vertical_angle),
		cos(vertical_angle) * cos(horizontal_angle)
	);

	right = glm::vec3(
		sin(horizontal_angle - M_PI / 2.0),
		0.0,
		cos(horizontal_angle - M_PI / 2.0)
	);

	up = glm::cross(right, direction);
}

void Camera::moveForward(double distance) {
	position += direction * static_cast<float>(distance);
}

void Camera::moveBackward(double distance) {
	moveForward(-distance);
}

void Camera::moveLeft(double distance) {
	moveRight(-distance);
}

void Camera::moveRight(double distance) {
	position += right * static_cast<float>(distance);
}

void Camera::update() {
	modelview_matrix = glm::lookAt(position, position + direction, up);
}
