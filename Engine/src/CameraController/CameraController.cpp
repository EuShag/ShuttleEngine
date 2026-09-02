/**
 * @file CameraController.cpp
 * @brief Implementation of CameraController.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "CameraController.hpp"

#include <iostream>
#include <ostream>

namespace shuttle::engine::core
{
    CameraController::CameraController(Camera& camera)
        : camera(camera)
    {}

    void CameraController::onKeyboard(const input::KeyboardEvent& event)
    {
        handleKeyboardEvent(event.key, event.state);
    }

    void CameraController::handleKeyboardEvent(pal::KeyCode key, pal::KeyState state)
    {
        const bool pressed = state == pal::KeyState::Pressed;

        switch (key)
        {
            case pal::KeyCode::W:
                pressed ? moveFlags |= Forward : moveFlags &= ~Forward;
                break;
            case pal::KeyCode::S:
                pressed ? moveFlags |= Backward : moveFlags &= ~Backward;
                break;
            case pal::KeyCode::A:
                pressed ? moveFlags |= Left : moveFlags &= ~Left;
                break;
            case pal::KeyCode::D:
                pressed ? moveFlags |= Right : moveFlags &= ~Right;
                break;
            case pal::KeyCode::LCtrl:
            case pal::KeyCode::RCtrl:
                pressed ? moveFlags |= Up : moveFlags &= ~Up;
                break;
            case pal::KeyCode::LShift:
            case pal::KeyCode::RShift:
                pressed ? moveFlags |= Down : moveFlags &= ~Down;
                break;
            case pal::KeyCode::Up:
                pressed ? rotateFlags |= PitchUp : rotateFlags &= ~PitchUp;
                break;
            case pal::KeyCode::Down:
                pressed ? rotateFlags |= PitchDown : rotateFlags &= ~PitchDown;
                break;
            case pal::KeyCode::Left:
                pressed ? rotateFlags |= YawLeft : rotateFlags &= ~YawLeft;
                break;
            case pal::KeyCode::Right:
                pressed ? rotateFlags |= YawRight : rotateFlags &= ~YawRight;
                break;
            default:
                break;
        }
    }

    void CameraController::update(float deltaTime, float movementSpeed, float rotationSpeed) const
    {
        glm::vec3 moveVec{0.0f};
        if (moveFlags & Forward)  moveVec.z -= movementSpeed;
        if (moveFlags & Backward) moveVec.z += movementSpeed;
        if (moveFlags & Left)     moveVec.x -= movementSpeed;
        if (moveFlags & Right)    moveVec.x += movementSpeed;
        if (moveFlags & Up)       moveVec.y += movementSpeed;
        if (moveFlags & Down)     moveVec.y -= movementSpeed;

        if (glm::length(moveVec) > 0.0f)
        {
            camera.moveLocal(moveVec, deltaTime);
        }

        float p = 0.0f, y = 0.0f;
        if (rotateFlags & PitchUp)   p += 1.0f;
        if (rotateFlags & PitchDown) p -= 1.0f;
        if (rotateFlags & YawLeft)   y += 1.0f;
        if (rotateFlags & YawRight)  y -= 1.0f;

        if (p != 0.0f || y != 0.0f)
        {
            camera.rotateEuler(p * rotationSpeed, y * rotationSpeed, 0.0f, deltaTime);
        }
    }
} // namespace shuttle::engine::core
