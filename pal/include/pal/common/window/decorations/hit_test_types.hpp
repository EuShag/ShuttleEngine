#pragma once
#include <cstdint>

namespace shuttle::pal
{
    // 1. Единый кроссплатформенный результат проверки попадания
    enum class WindowHitTestResult : uint8_t
    {
        Client,         // Клиентская область (UI, 3D-вьюпорт, меню)
        Caption,        // Перетаскивание окна за заголовок

        // Системные кнопки
        MinimizeButton,
        MaximizeButton,
        CloseButton,
        SystemMenu,

        // Границы изменения размеров
        ResizeTop,
        ResizeBottom,
        ResizeLeft,
        ResizeRight,
        ResizeTopLeft,
        ResizeTopRight,
        ResizeBottomLeft,
        ResizeBottomRight
    };

    // 2. Универсальный 2D Прямоугольник
    struct Rect
    {
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;

        [[nodiscard]] constexpr bool contains(int32_t px, int32_t py) const noexcept
        {
            return px >= x && px < (x + width) && py >= y && py < (y + height);
        }
    };
}
