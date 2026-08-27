#pragma once

namespace shuttle::pal
{
    /**
     * @brief Перечисление кнопок мыши.
     * Значения полностью независимы от Win32 API и SDL.
     */
    enum class MouseButton : uint8_t
    {
        None = 0,
        Left,
        Middle,
        Right,
        X1, // Дополнительная кнопка 1 (Назад)
        X2, // Дополнительная кнопка 2 (Вперед)

        // Служебное поле для размера массивов в InputSystem
        Count
    };
}
