#pragma once

#include <string>
#include <string_view>
#include <utility> // Для std::exchange в move-конструкторе

#include "PAL/Common/Window/WindowType.hpp"
#include "PAL/Common/Window/Decorations/WindowDecorations.hpp"

#if defined(_WIN32) && !defined(SHUTTLE_FORCE_SDL)
namespace shuttle::pal {
        namespace impl {
            class Win32Platform;
        }

        using Platform = impl::Win32Platform;
    }
#else
#define SHUTTLE_PLATFORM_SDL
namespace shuttle::pal {
    namespace impl {
        class SdlPlatform;
    }

    using Platform = impl::SdlPlatform;
    }
#endif

// Forward-декларации для слушателей
namespace shuttle::pal {
    class IWindowListener;
}
namespace shuttle::input {
    class IInputListener;
}

namespace shuttle::pal
{

    struct WindowHandle
    {
        uintptr_t value = 0;
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
        constexpr auto operator<=>(const WindowHandle&) const noexcept = default;
    };

    class WindowBase
    {
    public:
        // Конструктор: инициализируем платформу и хэндл
        WindowBase(
            Platform& platform,
            WindowType type,
            WindowHandle handle,
            std::string_view title = {},
            uint32_t width = 0,
            uint32_t height = 0,
            int32_t x = 0,
            int32_t y = 0);

        // Виртуальный деструктор для полиморфного удаления
        virtual ~WindowBase();

        // Запрет копирования
        WindowBase(const WindowBase&) = delete;
        WindowBase& operator=(const WindowBase&) = delete;

        // Поддержка перемещения (Move Semantics)
        WindowBase(WindowBase&& other) noexcept;
        // Move-assignment удаляется, т.к. m_platform - ссылка
        WindowBase& operator=(WindowBase&& other) noexcept = delete;

        // ---------------------------------------------------------------------
        // Прокси-методы (Пробрасывают вызовы в Платформу)
        // ---------------------------------------------------------------------
        void show();
        void hide();
        void maximize();
        void minimize();
        void restore();
        void close();

        void setPosition(int32_t x, int32_t y);
        void setSize(uint32_t width, uint32_t height);
        void setTitle(std::string_view title);

        virtual void setFullscreen(bool enabled);
        void setDropTarget(bool enabled);

        // ---------------------------------------------------------------------
        // Кроссплатформенные Геттеры
        // ---------------------------------------------------------------------
        [[nodiscard]] WindowHandle getHandle() const noexcept { return m_handle; }

        [[nodiscard]] Platform& platform() noexcept { return m_platform; }
        [[nodiscard]] const Platform& platform() const noexcept { return m_platform; }

        [[nodiscard]] WindowType getType() const noexcept { return m_type; }
        [[nodiscard]] std::string_view getTitle() const noexcept { return m_title; }

        [[nodiscard]] int32_t getX() const noexcept { return m_x; }
        [[nodiscard]] int32_t getY() const noexcept { return m_y; }
        [[nodiscard]] uint32_t getWidth() const noexcept { return m_width; }
        [[nodiscard]] uint32_t getHeight() const noexcept { return m_height; }
        [[nodiscard]] float getDpiScale() const noexcept { return m_dpiScale; }

        [[nodiscard]] bool isMaximized() const noexcept { return m_isMaximized; }
        [[nodiscard]] bool isMinimized() const noexcept { return m_isMinimized; }
        [[nodiscard]] bool isFocused() const noexcept { return m_isFocused; }
        [[nodiscard]] bool shouldClose() const noexcept { return m_shouldClose; }

        // Декорации
        void setDecorations(WindowDecorations decorations) { m_decorations = std::move(decorations); }
        [[nodiscard]] const WindowDecorations& getDecorations() const noexcept { return m_decorations; }
        [[nodiscard]] WindowDecorations& getDecorations() noexcept { return m_decorations; }

        // Слушатели
        void setWindowListener(IWindowListener* listener) noexcept { m_windowListener = listener; }
        void setInputListener(input::IInputListener* listener) noexcept { m_inputListener = listener; }

        [[nodiscard]] IWindowListener* getWindowListener() const noexcept { return m_windowListener; }
        [[nodiscard]] input::IInputListener* getInputListener() const noexcept { return m_inputListener; }

        // ---------------------------------------------------------------------
        // Внутреннее API Обновления (Вызывается ИСКЛЮЧИТЕЛЬНО платформой)
        // ---------------------------------------------------------------------
        void internalOnResize(uint32_t w, uint32_t h) noexcept;
        void internalOnMove(int32_t x, int32_t y) noexcept;
        void internalOnFocus(bool focused) noexcept;
        void internalOnDpiChanged(float scale) noexcept;
        void internalOnMaximized(bool maximized) noexcept;
        void internalOnMinimized(bool minimized) noexcept;
        void internalOnCloseRequested() noexcept;

    protected:
        Platform&     m_platform;
        WindowHandle  m_handle;
        WindowType    m_type;

        // Кэш состояния
        std::string   m_title;
        int32_t       m_x = 0;
        int32_t       m_y = 0;
        uint32_t      m_width = 0;
        uint32_t      m_height = 0;
        float         m_dpiScale = 1.0f;

        bool          m_isMaximized = false;
        bool          m_isMinimized = false;
        bool          m_isFocused = false;
        bool          m_shouldClose = false;

        WindowDecorations m_decorations{};

        IWindowListener*       m_windowListener = nullptr;
        input::IInputListener* m_inputListener = nullptr;
    };
}