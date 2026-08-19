#include "Camera.hpp"

namespace shuttle::engine::core
{
    Camera::Camera(glm::vec3 position, glm::quat orientation) : position(position), orientation(glm::normalize(orientation)) {}

    void Camera::moveLocal(glm::vec3 const& localDelta, float deltaTime)
    {
        position += (orientation * localDelta) * (movementSpeed * deltaTime);
    }

    void Camera::rotateEuler(float pitch, float yaw, float roll, float deltaTime)
    {
        float factor = rotationSpeed * deltaTime;
        glm::quat pitchQuat = glm::angleAxis(pitch * factor, glm::vec3{1.0f, 0.0f, 0.0f});
        glm::quat yawQuat = glm::angleAxis(yaw * factor, glm::vec3{0.0f, 1.0f, 0.0f});
        glm::quat rollQuat = glm::angleAxis(roll * factor, glm::vec3{0.0f, 0.0f, 1.0f});
        orientation = yawQuat * orientation * pitchQuat * rollQuat;
        orientation = glm::normalize(orientation);
    }

    glm::mat4 Camera::getViewMatrix() const
    {
        // Прямое вычисление: матрица вращения из сопряженного кватерниона * трансляция
        return glm::mat4_cast(glm::conjugate(orientation)) * glm::translate(glm::mat4{1.0f}, -position);
    }

    glm::mat4 Camera::getProjectionMatrix() const
    {
        glm::mat4 proj = glm::perspective(fov, aspectRatio, nearP, farP);
        proj[1][1] *= -1.0f;
        return proj;
    }

    void Camera::setWindowSize(uint32_t width, uint32_t height)
    {
        aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    }

    glm::quat Camera::getOrientation() const {
        return orientation;
    }

    float Camera::getFov() const {
        return fov;
    }

    float Camera::getAspectRatio() const {
        return aspectRatio;
    }

    float Camera::getNearPlane() const {
        return nearP;
    }

    float Camera::getFarPlane() const {
        return farP;
    }

    CameraData Camera::buildCameraData() const
    {
        const glm::mat4 viewMatrix = getViewMatrix();
        const glm::mat4 projectionMatrix = getProjectionMatrix();
        const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

        return {
            .viewMatrix = viewMatrix,
            .projectionMatrix = projectionMatrix,
            .viewProjectionMatrix = viewProjectionMatrix,
            .inverseViewMatrix = glm::inverse(viewMatrix),
            .inverseProjectionMatrix = glm::inverse(projectionMatrix),
            .inverseViewProjectionMatrix = glm::inverse(viewProjectionMatrix),
            .cameraPosition = glm::vec4(position, 1.0f),
            .nearPlane = nearP,
            .farPlane = farP,
            .fov = fov,
            .aspectRatio = aspectRatio
        };
    }

    void Camera::lookAt(glm::vec3 target, glm::vec3 up)
    {
        glm::vec3 forward = glm::normalize(target - position);
        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        glm::vec3 cameraUp = glm::cross(right, forward);
        glm::mat3 rotationMatrix(right, cameraUp, -forward);
        orientation = glm::normalize(glm::quat_cast(rotationMatrix));
    }

    // Реализация пропущенного метода
    void Camera::setProjection(float fovDeg, float aspect, float nearPlane, float farPlane)
    {
        fov = glm::radians(fovDeg);
        aspectRatio = aspect;
        nearP = nearPlane;
        farP = farPlane;
    }
} // namespace shuttle::engine::core
