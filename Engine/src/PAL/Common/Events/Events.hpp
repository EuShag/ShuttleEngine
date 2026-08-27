#pragma once
#include <cstdint>
#include <bitset>

#include "PAL/Common/Keyboard/KeyCode.hpp"
#include "PAL/Common/Keyboard/KeyMode.hpp"
#include "PAL/Common/Keyboard/KeyState.hpp"
#include "PAL/Common/Mouse/MouseButton.hpp"
#include "PAL/Common/Window/ShowMode.hpp"

namespace shuttle::pal
{
    // =========================================================================
    // 1. СТРУКТУРЫ ДАННЫХ СОБЫТИЙ ОКНА (Lifecycle & Geometry)
    // =========================================================================
    struct WindowResizeEvent     { uint32_t width; uint32_t height; };
    struct WindowMoveEvent       { int32_t x; int32_t y; };
    struct WindowFocusEvent      { bool focused; };
    struct WindowShowModeEvent   { ShowMode mode; };
    struct WindowDpiChangedEvent { float scale; uint32_t newWidth; uint32_t newHeight; };
    struct WindowCloseEvent      {}; // Запрос на закрытие
    struct WindowDropFilesEvent  { int32_t x; int32_t y; std::vector<std::string> filePaths; };

    // =========================================================================
    // 2. ИНТЕРФЕЙС СЛУШАТЕЛЯ СОСТОЯНИЯ ОКНА (IWindowListener)
    // =========================================================================
    // Реализуется Рендерером, UI-менеджером или Application
    class IWindowListener
    {
    public:
        virtual ~IWindowListener() = default;

        virtual void onWindowResize(const WindowResizeEvent& e) {}
        virtual void onWindowMove(const WindowMoveEvent& e) {}
        virtual void onWindowShowModeChanged(const WindowShowModeEvent& e) {}
        virtual void onWindowFocusChanged(const WindowFocusEvent& e) {}
        virtual void onWindowDpiChanged(const WindowDpiChangedEvent& e) {}
        virtual void onWindowDropFiles(const WindowDropFilesEvent& e) {}
        virtual void onWindowPaint() {} // Системная перерисовка (WM_PAINT)
        virtual void onWindowCloseRequested() {}
    };
}

namespace shuttle::input
{
    // =========================================================================
    // 3. СТРУКТУРЫ ДАННЫХ ВВОДА (Raw Input Data)
    // =========================================================================
    struct KeyboardEvent
    {
        pal::KeyCode  key = pal::KeyCode::Unknown;
        pal::KeyMode  mode{};
        pal::KeyState state = pal::KeyState::Released;
    };

    struct MouseButtonEvent
    {
        pal::MouseButton button = pal::MouseButton::None;
        pal::KeyState    state = pal::KeyState::Released;
    };

    struct MouseMoveEvent
    {
        int32_t x = 0;
        int32_t y = 0;
    };

    struct MouseWheelEvent
    {
        float delta = 0.0f;
    };

    // =========================================================================
    // 4. ИНТЕРФЕЙС СЛУШАТЕЛЯ ВВОДА (IInputListener)
    // =========================================================================
    // Реализуется ИСКЛЮЧИТЕЛЬНО InputSystem
    class IInputListener
    {
    public:
        virtual ~IInputListener() = default;

        virtual void onKeyboard(const KeyboardEvent& e) = 0;
        virtual void onMouseButton(const MouseButtonEvent& e) = 0;
        virtual void onMouseMove(const MouseMoveEvent& e) = 0;
        virtual void onMouseWheel(const MouseWheelEvent& e) = 0;
    };

    // =========================================================================
    // 5. СОСТОЯНИЕ ВВОДА (InputState)
    // =========================================================================
    struct InputState
    {
        std::bitset<512> keys;
        std::bitset<512> previousKeys;

        std::bitset<8>   mouseButtons;
        std::bitset<8>   previousMouseButtons;

        int32_t mouseX = 0;
        int32_t mouseY = 0;
        int32_t mouseDeltaX = 0;
        int32_t mouseDeltaY = 0;
        float   scrollDelta = 0.0f;
    };

    // =========================================================================
    // 6. СИСТЕМА ВВОДА (InputSystem)
    // =========================================================================
    class InputSystem : public IInputListener
    {
    public:
        InputSystem() = default;

        // Вызывается в начале каждого кадра в основном цикле
        void update() noexcept;

        // --- Polling API (Для использования в коде игры/движка) ---
        [[nodiscard]] bool isKeyDown(pal::KeyCode key) const noexcept;
        [[nodiscard]] bool isKeyPressed(pal::KeyCode key) const noexcept;
        [[nodiscard]] bool isKeyReleased(pal::KeyCode key) const noexcept;

        [[nodiscard]] bool isMouseButtonDown(pal::MouseButton button) const noexcept;
        [[nodiscard]] bool isMouseButtonPressed(pal::MouseButton button) const noexcept;

        void getMousePosition(int32_t& x, int32_t& y) const noexcept;
        void getMouseDelta(int32_t& dx, int32_t& dy) const noexcept;
        [[nodiscard]] float getScrollDelta() const noexcept;

        // --- Реализация IInputListener (Слушает события от Win32WindowBase) ---
        void onKeyboard(const KeyboardEvent& e) override;
        void onMouseButton(const MouseButtonEvent& e) override;
        void onMouseMove(const MouseMoveEvent& e) override;
        void onMouseWheel(const MouseWheelEvent& e) override;

    private:
        InputState m_state;
    };
}
