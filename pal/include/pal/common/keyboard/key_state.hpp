#pragma once

#include <cstdint>

namespace shuttle::pal
{
    /**
     * @brief Состояние клавиши или кнопки мыши.
     */
    enum class KeyState : uint8_t
    {
        Released = 0, // Клавиша/кнопка отпущена
        Pressed  = 1  // Клавиша/кнопка нажата
    };
}
