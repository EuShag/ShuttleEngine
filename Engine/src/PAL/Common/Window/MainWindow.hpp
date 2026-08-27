#pragma once

#include "WindowBase.hpp"

namespace shuttle::pal
{
    class MainWindow final : public WindowBase
    {
    public:
        MainWindow(
            Platform& platform,
            WindowHandle handle,
            std::string_view title = "Shuttle Engine",
            uint32_t width = 1280,
            uint32_t height = 720)
            : WindowBase(platform, WindowType::Main, handle, title, width, height)
        {
        }

        ~MainWindow() override = default;

        // Запрет копирования, поддержка перемещения (наследуется от WindowBase, но явно пропишем для ясности)
        MainWindow(const MainWindow&) = delete;
        MainWindow& operator=(const MainWindow&) = delete;
        MainWindow(MainWindow&&) noexcept = default;
        MainWindow& operator=(MainWindow&&) noexcept = delete;

        // ---------------------------------------------------------------------
        // Специфичный для главного окна функционал (Fullscreen)
        // ---------------------------------------------------------------------

        void setFullscreen(bool enabled) override
        {
            if (m_isFullscreen != enabled)
            {
                m_isFullscreen = enabled;
                // Базовый класс WindowBase уже имеет метод проброса в платформу
                WindowBase::setFullscreen(m_isFullscreen);
            }
        }

        void toggleFullscreen()
        {
            setFullscreen(!m_isFullscreen);
        }

        [[nodiscard]] bool isFullscreen() const noexcept
        {
            return m_isFullscreen;
        }

    private:
        bool m_isFullscreen = false;
    };
}
