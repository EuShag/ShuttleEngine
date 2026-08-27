/**
 * @file CameraController.hpp
 * @brief Cross-platform camera movement and rotation controller.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "../Camera/Camera.hpp"
#include "PAL/Common/Events/Events.hpp"

namespace shuttle::engine::core
{
    /**
     * @class CameraController
     * @brief Manages camera translation and rotation based on PAL keyboard input events.
     */
    class CameraController : public input::IInputListener
    {
    public:
        explicit CameraController(Camera& camera);

        /**
         * @brief Handles keyboard input event from PAL platform.
         * @param event The keyboard event structure.
         */
        void onKeyboard(const input::KeyboardEvent& event) override;
        void onMouseButton(const input::MouseButtonEvent& e) override {};
        void onMouseMove(const input::MouseMoveEvent& e) override {};
        void onMouseWheel(const input::MouseWheelEvent& e) override {};

        /**
         * @brief Legacy helper method for direct keyboard event handling.
         * @param key Pressed/released key.
         * @param state Key state (Pressed or Released).
         */
        void handleKeyboardEvent(pal::KeyCode key, pal::KeyState state);

        /**
         * @brief Updates camera position and orientation.
         * @param deltaTime Delta time since last frame in seconds.
         * @param movementSpeed Movement speed factor.
         * @param rotationSpeed Rotation speed factor.
         */
        void update(float deltaTime, float movementSpeed, float rotationSpeed) const;

    private:
        Camera& camera;

        enum MoveDir : uint32_t {
            None     = 0,
            Forward  = 1 << 0,
            Backward = 1 << 1,
            Left     = 1 << 2,
            Right    = 1 << 3,
            Up       = 1 << 4,
            Down     = 1 << 5
        };

        enum RotateDir : uint32_t {
            NoRot     = 0,
            PitchUp   = 1 << 0,
            PitchDown = 1 << 1,
            YawLeft   = 1 << 2,
            YawRight  = 1 << 3
        };

        mutable uint32_t moveFlags = None;
        mutable uint32_t rotateFlags = NoRot;
    };
} // namespace shuttle::engine::core
