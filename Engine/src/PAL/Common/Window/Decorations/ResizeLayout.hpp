#pragma once
#include "HitTestTypes.hpp"

namespace shuttle::pal
{
    class ResizeLayout
    {
    public:
        uint32_t borderThickness = 8; // Ширина зоны перехвата в пикселях
        bool isResizable = true;      // Можно ли менять размер?

        // Кроссплатформенный алгоритм проверки рамок ресайза
        [[nodiscard]] WindowHitTestResult evaluate(int32_t localX, int32_t localY,
                                                   int32_t windowWidth, int32_t windowHeight) const noexcept
        {
            if (!isResizable)
            {
                return WindowHitTestResult::Client;
            }

            const auto thick = static_cast<int32_t>(borderThickness);

            const bool top    = localY < thick;
            const bool bottom = localY >= (windowHeight - thick);
            const bool left   = localX < thick;
            const bool right  = localX >= (windowWidth - thick);

            if (top && left)     return WindowHitTestResult::ResizeTopLeft;
            if (top && right)    return WindowHitTestResult::ResizeTopRight;
            if (bottom && left)  return WindowHitTestResult::ResizeBottomLeft;
            if (bottom && right) return WindowHitTestResult::ResizeBottomRight;
            if (top)             return WindowHitTestResult::ResizeTop;
            if (bottom)          return WindowHitTestResult::ResizeBottom;
            if (left)            return WindowHitTestResult::ResizeLeft;
            if (right)           return WindowHitTestResult::ResizeRight;

            return WindowHitTestResult::Client;
        }
    };
}
