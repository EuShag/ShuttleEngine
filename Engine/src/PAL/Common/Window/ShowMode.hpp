#pragma once
#include <cstdint> // Для базового типа uint8_t

namespace shuttle::pal
{
    // Определяет текущее состояние отображения окна
    enum class ShowMode : uint8_t
    {
        Normal,      // Обычное, неразвернутое/несвернутое окно
        Minimized,   // Свернутое в панель задач
        Maximized,   // Развернутое на весь экран (рабочая область)
        Fullscreen   // Полноэкранный режим (без рамок и панели задач)
    };
}
