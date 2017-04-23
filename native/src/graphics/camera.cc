// Copyright (C) 2017 Chris Liebert
#include "graphics/camera.h"

Camera::Camera() {
	modelview_matrix = glm::mat4(1.0);
	projection_matrix = glm::mat4(1.0);
	horizontal_angle = M_PI; //3.1415926539
	vertical_angle = 0.0;
	position = glm::vec3(0.0, 0.0, 0.0);
	aim(0.0, 0.0);
	update();
}

void Camera::aim(double x, double y) {
	horizontal_angle += x;
	vertical_angle += y;

	direction = glm::vec3(cos(vertical_angle) * sin(horizontal_angle),
			sin(vertical_angle), cos(vertical_angle) * cos(horizontal_angle));
	right = glm::vec3(sin(horizontal_angle - M_PI / 2.0), 0.0,
			cos(horizontal_angle - M_PI / 2.0));

	up = glm::cross(right, direction);
}

void Camera::moveForward(double amount) {
	glm::vec3 scaledDirection = glm::vec3(direction.x * amount,
			direction.y * amount, direction.z * amount);
	position += scaledDirection;
}

void Camera::moveBackward(double amount) {
	moveForward(amount * -1.0);
}

void Camera::moveLeft(double amount) {
	moveRight(amount * -1.0);
}

void Camera::moveRight(double amount) {
	glm::vec3 scaledRight = glm::vec3(right.x * amount, right.y * amount,
			right.z * amount);
	position += scaledRight;
}

void Camera::update() {
	modelview_matrix = glm::lookAt(position, position + direction, up);
}
