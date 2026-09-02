// Включаем Platform.hpp, чтобы видеть полные определения методов платформы
#include "PAL/Platform.hpp"
#include "PAL/Common/Window/WindowBase.hpp"
#include "PAL/Common/Events/Events.hpp" // Добавлено, если IWindowListener нужен здесь
#include <utility> // Для std::exchange, std::move

#include "imgui.h"


namespace shuttle::pal
{
    WindowBase::WindowBase(Platform& platform, WindowType type, WindowHandle handle,
                           std::string_view title, uint32_t width, uint32_t height,
                           int32_t x, int32_t y)
        : m_platform(platform)
        , m_handle(handle)
        , m_type(type)
        , m_title(title)
        , m_x(x)
        , m_y(y)
        , m_width(width)
        , m_height(height)
    {
        // Вызываем bindWindow, чтобы связать нативный HWND с С++ указателем "this"
        m_platform.bindWindow(m_handle, this);
    }

    WindowBase::~WindowBase()
    {
        m_imguiContext = nullptr; // Обнуляем ImGui контекст, если он был создан

        // Если окно уничтожается С++ деструктором (например, при выходе из области видимости),
        // просим платформу физически закрыть нативный дескриптор.
        // Проверяем m_handle, т.к. оно может быть обнулено при move-семантике
        if (m_handle)
        {
            m_platform.destroyWindow(m_handle);
        }
    }

    WindowBase::WindowBase(WindowBase&& other) noexcept
        : m_platform(other.m_platform) // Ссылка копируется
        // Забираем хэндл и обнуляем его у старого объекта
        , m_handle(std::exchange(other.m_handle, {}))
        , m_type(other.m_type)
        , m_title(std::move(other.m_title))
        , m_x(other.m_x)
        , m_y(other.m_y)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_dpiScale(other.m_dpiScale)
        , m_isMaximized(other.m_isMaximized)
        , m_isMinimized(other.m_isMinimized)
        , m_isFocused(other.m_isFocused)
        , m_shouldClose(other.m_shouldClose)
        , m_decorations(std::move(other.m_decorations))
        , m_windowListener(std::exchange(other.m_windowListener, nullptr)) // Перемещаем слушателей
        , m_inputListener(std::exchange(other.m_inputListener, nullptr))
    {
        // Перерегистрируем новый адрес перемещенного объекта в памяти платформы
        if (m_handle)
        {
            m_platform.bindWindow(m_handle, this);
        }
    }

    // Move-assignment оператор удален в .hpp из-за ссылочного члена.
    // Если бы m_platform был указателем, его можно было бы реализовать.
    // WindowBase & WindowBase::operator=(WindowBase &&other) noexcept { ... }

    // ---------------------------------------------------------------------
    // Реализация прокси-методов
    // ---------------------------------------------------------------------

    void WindowBase::show()
    {
        m_platform.showWindow(m_handle);
    }

    void WindowBase::hide()
    {
        m_platform.hideWindow(m_handle);
    }

    void WindowBase::maximize()
    {
        m_platform.maximizeWindow(m_handle);
    }

    void WindowBase::minimize()
    {
        m_platform.minimizeWindow(m_handle);
    }

    void WindowBase::restore()
    {
        m_platform.restoreWindow(m_handle);
    }

    void WindowBase::close()
    {
        m_shouldClose = true;
        m_platform.destroyWindow(m_handle);
    }

    void WindowBase::setPosition(int32_t x, int32_t y)
    {
        m_platform.setWindowPosition(m_handle, x, y);
    }

    void WindowBase::setSize(uint32_t width, uint32_t height)
    {
        m_platform.setWindowSize(m_handle, width, height);
    }

    void WindowBase::setTitle(std::string_view title)
    {
        m_title = title;
        m_platform.setWindowTitle(m_handle, title);
    }

    void WindowBase::setFullscreen(bool enabled)
    {
        m_platform.setFullscreen(m_handle, enabled);
    }

    void WindowBase::setDropTarget(bool enabled)
    {
        m_platform.setWindowDropTarget(m_handle, enabled);
    }

    // ---------------------------------------------------------------------
    // Реализация методов обновления состояния (вызываются из WndProc)
    // ---------------------------------------------------------------------

    void WindowBase::internalOnResize(uint32_t w, uint32_t h) noexcept
    {
        m_width = w;
        m_height = h;
    }

    void WindowBase::internalOnMove(int32_t x, int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
    }

    void WindowBase::internalOnFocus(bool focused) noexcept
    {
        m_isFocused = focused;
    }

    void WindowBase::internalOnDpiChanged(float scale) noexcept
    {
        m_dpiScale = scale;
    }

    void WindowBase::internalOnMaximized(bool maximized) noexcept
    {
        m_isMaximized = maximized;
        if (maximized) m_isMinimized = false;
    }

    void WindowBase::internalOnMinimized(bool minimized) noexcept
    {
        m_isMinimized = minimized;
        if (minimized) m_isMaximized = false;
    }

    void WindowBase::internalOnCloseRequested() noexcept
    {
        m_shouldClose = true;
    }
}
