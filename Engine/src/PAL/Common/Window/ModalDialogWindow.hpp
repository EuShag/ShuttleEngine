#pragma once

#include "WindowBase.hpp"
#include <functional>
#include <utility>

namespace shuttle::pal
{
    class ModalDialogWindow final : public WindowBase
    {
    public:
        using ConfirmCallback = std::function<void()>;
        using CancelCallback  = std::function<void()>;

        ModalDialogWindow(
            Platform& platform,
            WindowHandle handle,
            std::string_view title = "Dialog",
            uint32_t width = 600,
            uint32_t height = 400)
            : WindowBase(platform, WindowType::ModalDialog, handle, title, width, height)
        {
        }

        ~ModalDialogWindow() override
        {
            // Если нужно выполнить специфичную логику при закрытии диалога
            // (например, убедиться, что родительское окно разблокировано)
            cancel();
        }

        // Запрет копирования, поддержка перемещения
        ModalDialogWindow(const ModalDialogWindow&) = delete;
        ModalDialogWindow& operator=(const ModalDialogWindow&) = delete;
        ModalDialogWindow(ModalDialogWindow&&) noexcept = default;
        ModalDialogWindow& operator=(ModalDialogWindow&&) noexcept = delete;

        // Установка коллбэков
        void setOnConfirm(ConfirmCallback callback) { m_onConfirm = std::move(callback); }
        void setOnCancel(CancelCallback callback)   { m_onCancel = std::move(callback); }

        // Вызывается из UI при нажатии кнопок подтверждения/отмены
        void confirm()
        {
            if (m_onConfirm)
            {
                m_onConfirm();
            }
            close(); // Закрываем окно после подтверждения
        }

        void cancel()
        {
            if (m_onCancel)
            {
                m_onCancel();
            }
            close(); // Закрываем окно после отмены
        }

    private:
        ConfirmCallback m_onConfirm;
        CancelCallback  m_onCancel;
    };
}
