#pragma once
#include "WindowDecorationFlags.hpp"
#include "ResizeLayout.hpp"
#include "TitlebarLayout.hpp"

namespace shuttle::pal
{
    class WindowDecorations
    {
    public:
        WindowDecorationFlags flags = WindowDecorationFlags::Default;
        TitlebarLayout        titlebar;
        ResizeLayout          resize;

        // Единый алгоритм с учетом флагов
        [[nodiscard]] WindowHitTestResult evaluate(int32_t localX, int32_t localY,
                                                   int32_t windowWidth, int32_t windowHeight,
                                                   bool isMaximized) const noexcept
        {
            // 1. Проверяем ресайз только если включен флаг Resizable
            if (!isMaximized && hasFlag(flags, WindowDecorationFlags::Resizable))
            {
                WindowHitTestResult resizeResult = resize.evaluate(localX, localY, windowWidth, windowHeight);
                if (resizeResult != WindowHitTestResult::Client)
                {
                    return resizeResult;
                }
            }

            // 2. Проверяем заголовок только если включен флаг Titlebar
            if (hasFlag(flags, WindowDecorationFlags::Titlebar))
            {
                WindowHitTestResult titlebarResult = titlebar.evaluate(localX, localY);

                // Фильтруем результаты в соответствии с флагами:
                if (titlebarResult == WindowHitTestResult::MaximizeButton &&
                    !hasFlag(flags, WindowDecorationFlags::MaximizeButton))
                {
                    return WindowHitTestResult::Caption; // Кнопки нет, считаем просто заголовком
                }

                if (titlebarResult == WindowHitTestResult::MinimizeButton &&
                    !hasFlag(flags, WindowDecorationFlags::MinimizeButton))
                {
                    return WindowHitTestResult::Caption; // Кнопки нет
                }

                if (titlebarResult != WindowHitTestResult::Client)
                {
                    return titlebarResult;
                }
            }

            return WindowHitTestResult::Client;
        }
    };
}
