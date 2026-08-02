#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace shuttle
{
class Camera
{
  public:
    Camera(glm::vec3 position = glm::vec3{0.0f, 0.0f, 5.0f}, glm::quat orientation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
    void moveLocal(glm::vec3 const& localDelta, float deltaTime);
    void rotateEuler(float pitch, float yaw, float roll, float deltaTime);
    void lookAt(glm::vec3 target, glm::vec3 up = glm::vec3{0.0f, 1.0f, 0.0f});
    void setProjection(float fovDeg, float aspect, float nearPlane, float farPlane);
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix() const;
    [[nodiscard]] glm::mat4 getShortProjectionMatrix() const;

    [[nodiscard]] glm::vec3 getPosition() const { return position; }

    void setWindowSize(uint32_t width, uint32_t height);

    float movementSpeed = 5.0f;
    float rotationSpeed = 1.5f;

  private:
    glm::quat orientation;
    glm::vec3 position;
    float fov = glm::radians(45.0f);
    float aspectRatio = 16.0f / 9.0f;
    float nearP = 0.1f;
    float farP = 1000.0f;
};
} // namespace shuttle
