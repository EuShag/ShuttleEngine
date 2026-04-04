#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vm {
	class Camera {
	public:
		Camera(glm::vec3 position = glm::vec3{1.0f}, glm::quat orientation = glm::quat{});

		void move(glm::vec3 delta);
		void rotate(float angle, glm::vec3 axis);
		void lookAt(glm::vec3 target, glm::vec3 up);

		glm::mat4 getViewMatrix() const;

	private:
		glm::quat orientation;
		glm::vec3 position;
		glm::vec3 up{ 0.0f, 1.0f, 0.0f };
	};
}