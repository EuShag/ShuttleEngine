#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace shuttle::engine::core {
    struct CameraData {
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 viewProjectionMatrix;

        glm::mat4 inverseViewMatrix;
        glm::mat4 inverseProjectionMatrix;
        glm::mat4 inverseViewProjectionMatrix;

        glm::vec4 cameraPosition;

        float nearPlane;
        float farPlane;

        float fov;
        float aspectRatio;
    };

    class Camera {
    public:
        Camera(glm::vec3 position = glm::vec3{0.0f, 0.0f, 5.0f}, glm::quat orientation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
        void moveLocal(glm::vec3 const& localDelta, float deltaTime);
        void rotateEuler(float pitch, float yaw, float roll, float deltaTime);
        void lookAt(glm::vec3 target, glm::vec3 up = glm::vec3{0.0f, 1.0f, 0.0f});
        void setProjection(float fovDeg, float aspect, float nearPlane, float farPlane);
        [[nodiscard]] glm::mat4 getViewMatrix() const;
        [[nodiscard]] glm::mat4 getProjectionMatrix() const;

        [[nodiscard]] glm::vec3 getPosition() const { return position; }

        void setWindowSize(uint32_t width, uint32_t height);

        [[nodiscard]] glm::quat getOrientation() const;

        [[nodiscard]] float getFov() const;
        [[nodiscard]] float getAspectRatio() const;

        [[nodiscard]] float getNearPlane() const;
        [[nodiscard]] float getFarPlane() const;

        [[nodiscard]] CameraData buildCameraData() const;

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
} // namespace shuttle::engine::core
