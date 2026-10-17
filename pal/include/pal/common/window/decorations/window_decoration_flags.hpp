#pragma once
#include <cstdint>
#include <type_traits>

namespace shuttle::pal
{
    enum class WindowDecorationFlags : uint32_t
    {
        None            = 0,
        Titlebar        = 1 << 0, // Наличие зоны заголовка
        CloseButton     = 1 << 1, // Кнопка "Закрыть"
        MinimizeButton  = 1 << 2, // Кнопка "Свернуть"
        MaximizeButton  = 1 << 3, // Кнопка "Развернуть" (Snap Layouts)
        Resizable       = 1 << 4, // Границы изменения размера (Thick Frame)
        SystemMenu      = 1 << 5, // Иконка системного меню

        // Пресеты для быстрого создания окон:

        // Главное окно: есть всё!
        Default = Titlebar | CloseButton | MinimizeButton | MaximizeButton | Resizable | SystemMenu,

        // Модальный диалог: заголовок, закрыть, без ресайза и сворачивания
        Dialog = Titlebar | CloseButton,

        // Отцепленная панель: заголовок, закрыть, ресайз, без сворачивания
        ToolWindow = Titlebar | CloseButton | Resizable,

        // Полностью без рамок и элементов
        Borderless = None
    };

    // Битовые операторы для удобной работы с флагами (| , & , ~)
    constexpr WindowDecorationFlags operator|(WindowDecorationFlags a, WindowDecorationFlags b) noexcept {
        return static_cast<WindowDecorationFlags>(
            static_cast<std::underlying_type_t<WindowDecorationFlags>>(a) |
            static_cast<std::underlying_type_t<WindowDecorationFlags>>(b)
        );
    }

    constexpr WindowDecorationFlags operator&(WindowDecorationFlags a, WindowDecorationFlags b) noexcept {
        return static_cast<WindowDecorationFlags>(
            static_cast<std::underlying_type_t<WindowDecorationFlags>>(a) &
            static_cast<std::underlying_type_t<WindowDecorationFlags>>(b)
        );
    }

    constexpr bool hasFlag(WindowDecorationFlags flags, WindowDecorationFlags flag) noexcept {
        return (flags & flag) == flag;
    }
}
