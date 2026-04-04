#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

vm::Camera::Camera(glm::vec3 position, glm::quat orientation)
	: position(position), orientation(orientation) {}

void vm::Camera::move(glm::vec3 delta) {
	position += orientation * delta;
}

void vm::Camera::rotate(float angle, glm::vec3 axis) {
	auto cameraForward = orientation * glm::vec3{ 0.0f, 0.0f, -1.0f };
	auto cameraRight = orientation * glm::vec3{ 1.0f, 0.0f, 0.0f };
	auto cameraUp = orientation * glm::vec3{ 0.0f, 1.0f, 0.0f };

	auto worldAxis = glm::normalize(cameraRight * axis.x + cameraUp * axis.y + cameraForward * axis.z);

	orientation = glm::angleAxis(angle, worldAxis) * orientation;
	orientation = glm::normalize(orientation);
}

glm::mat4 vm::Camera::getViewMatrix() const {
	auto rotationMatrix = glm::inverse(glm::rotate(glm::mat4{ 1.0f }, glm::angle(glm::normalize(orientation)), glm::axis(glm::normalize(orientation))));
	auto translateMatrix = glm::translate(glm::mat4{1.0}, - position);
	return rotationMatrix * translateMatrix;
}

void vm::Camera::lookAt(glm::vec3 target, glm::vec3 up) {
	auto forward = glm::normalize(target - position);
	auto right = glm::normalize(glm::cross(forward, up));
	auto cameraUp = glm::cross(right, forward);

	glm::mat3 rotationMatrix(right, cameraUp, -forward);

	orientation = glm::quat_cast(rotationMatrix);
}