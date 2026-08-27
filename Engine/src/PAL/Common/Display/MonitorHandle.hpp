#pragma once

#include <cstdint>
#include <compare>

namespace shuttle::pal
{
    struct MonitorHandle
    {
        // На Windows внутри хранится HMONITOR,
        // на SDL/Linux — платформенный идентификатор дисплея.
        uintptr_t value = 0;

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept
        {
            return value != 0;
        }

        constexpr auto operator<=>(MonitorHandle const&) const noexcept = default;
    };
}
