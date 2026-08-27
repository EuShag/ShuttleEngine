#pragma once

#include <string>
#include <vector>

#include "PAL/Common/Display/DisplayTypes.hpp"
#include "PAL/Common/Display/MonitorHandle.hpp"
#include "PAL/Common/Window/Decorations/HitTestTypes.hpp"

namespace shuttle::pal
{
    struct MonitorInfo
    {
        // Идентификатор конкретного дисплея.
        // Не является владельцем ресурса и может устареть после
        // переподключения монитора.
        MonitorHandle handle{};

        // Человекочитаемое имя монитора.
        std::string name{"Unknown Monitor"};

        // Системный идентификатор устройства.
        // Например, \\.\DISPLAY1 в Win32.
        std::string deviceName{};

        // Координаты в виртуальном пространстве рабочего стола.
        Rect bounds{};

        // Рабочая область без панели задач и прочих системных областей.
        Rect workArea{};

        // Коэффициент DPI: 1.0 = 100%, 1.25 = 125% и т. д.
        float dpiScale = 1.0f;

        // Основной монитор системы.
        bool isPrimary = false;

        // Текущий режим вывода.
        DisplayMode currentMode{};

        // Нативный или предпочтительный режим.
        DisplayMode nativeMode{};

        // Поддерживаемые режимы на момент получения информации.
        std::vector<DisplayMode> availableModes{};

        // Возможности HDR.
        HdrCapabilities hdr{};

        // Текущая ориентация.
        DisplayOrientation orientation =
            DisplayOrientation::Landscape;
    };
}
