#include "CameraController.hpp"

namespace shuttle::engine::core
{
    CameraController::CameraController(Camera& camera) : camera(camera)
    {}

    void CameraController::handleKeyboardEvent(SdlWindow &, SdlKeyCode keyCode, SdlKeyMode, SdlKeyState keyState, SdlLibrary&)
    {
        bool pressed = (keyState == SdlKeyState::Pressed);
        switch (keyCode)
        {
        case SdlKeyCode::W: (pressed ? moveFlags |= Forward : moveFlags &= ~Forward); break;
        case SdlKeyCode::S: (pressed ? moveFlags |= Backward : moveFlags &= ~Backward); break;
        case SdlKeyCode::A: (pressed ? moveFlags |= Left : moveFlags &= ~Left); break;
        case SdlKeyCode::D: (pressed ? moveFlags |= Right : moveFlags &= ~Right); break;
        case SdlKeyCode::Q: (pressed ? moveFlags |= Up : moveFlags &= ~Up); break;
        case SdlKeyCode::E: (pressed ? moveFlags |= Down : moveFlags &= ~Down); break;
        case SdlKeyCode::Up: (pressed ? rotateFlags |= PitchUp : rotateFlags &= ~PitchUp); break;
        case SdlKeyCode::Down: (pressed ? rotateFlags |= PitchDown : rotateFlags &= ~PitchDown); break;
        case SdlKeyCode::Left: (pressed ? rotateFlags |= YawLeft : rotateFlags &= ~YawLeft); break;
        case SdlKeyCode::Right: (pressed ? rotateFlags |= YawRight : rotateFlags &= ~YawRight); break;
        default: break;
        }
    }

    void CameraController::update(float deltaTime, float movementSpeed, float rotationSpeed) const
    {
        glm::vec3 moveVec{0.0f};
        if (moveFlags & Forward) moveVec.z -= movementSpeed;
        if (moveFlags & Backward) moveVec.z += movementSpeed;
        if (moveFlags & Left) moveVec.x -= movementSpeed;
        if (moveFlags & Right) moveVec.x += movementSpeed;
        if (moveFlags & Up) moveVec.y += movementSpeed;
        if (moveFlags & Down) moveVec.y -= movementSpeed;

        if (glm::length(moveVec) > 0.0f) camera.moveLocal(moveVec, deltaTime);

        float p = 0, y = 0;
        if (rotateFlags & PitchUp) p += 1.0f;
        if (rotateFlags & PitchDown) p -= 1.0f;
        if (rotateFlags & YawLeft) y += 1.0f;
        if (rotateFlags & YawRight) y -= 1.0f;
        camera.rotateEuler(p * rotationSpeed, y * rotationSpeed, 0.0f, deltaTime);
    }
} // namespace shuttle
