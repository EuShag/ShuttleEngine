#pragma once
#include "../Camera/Camera.hpp"
#include "Sdl.hpp"

namespace shuttle::engine::core
{
    class CameraController
    {
    public:
        explicit CameraController(Camera& camera);
        void handleKeyboardEvent(SdlWindow &window, SdlKeyCode keyCode, SdlKeyMode keyMode, SdlKeyState keyState, SdlLibrary&);
        void update(float deltaTime, float movementSpeed, float rotationSpeed) const;

    private:
        Camera& camera;

        enum MoveDir : uint32_t {
            None = 0,
            Forward = 1 << 0,
            Backward = 1 << 1,
            Left = 1 << 2,
            Right = 1 << 3,
            Up = 1 << 4,
            Down = 1 << 5
        };

        enum RotateDir : uint32_t {
            NoRot = 0,
            PitchUp = 1 << 0,
            PitchDown = 1 << 1,
            YawLeft = 1 << 2,
            YawRight = 1 << 3
        };

        uint32_t moveFlags = None;
        uint32_t rotateFlags = NoRot;
    };
} // namespace shuttle
