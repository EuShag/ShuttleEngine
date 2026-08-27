#pragma once

#include "WindowBase.hpp"

namespace shuttle::pal
{
    class DetachedPanelWindow final : public WindowBase
    {
    public:
        DetachedPanelWindow(
            Platform& platform,
            WindowHandle handle,
            std::string_view title = "Panel",
            uint32_t width = 300,
            uint32_t height = 600)
            : WindowBase(platform, WindowType::DetachedPanel, handle, title, width, height)
        {
        }

        ~DetachedPanelWindow() override = default;

        // Запрет копирования, поддержка перемещения
        DetachedPanelWindow(const DetachedPanelWindow&) = delete;
        DetachedPanelWindow& operator=(const DetachedPanelWindow&) = delete;
        DetachedPanelWindow(DetachedPanelWindow&&) noexcept = default;
        DetachedPanelWindow& operator=(DetachedPanelWindow&&) noexcept = delete;

        // Специфичный функционал панелей: прятать при закрытии вместо уничтожения
        // (Это значение будет учитываться в WndProc, который мы написали)
        void setHideOnClose(bool hideOnClose) noexcept
        {
            m_hideOnClose = hideOnClose;
        }

        [[nodiscard]] bool isHideOnClose() const noexcept
        {
            return m_hideOnClose;
        }

    private:
        // По умолчанию панели просто прячутся, чтобы их можно было быстро вернуть в UI
        bool m_hideOnClose = true;
    };
}
