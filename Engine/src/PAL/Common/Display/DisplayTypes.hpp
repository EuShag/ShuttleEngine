#pragma once

#include <array>
#include <cstdint>

namespace shuttle::pal
{
    struct DisplayMode
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRateNumerator = 0;
        uint32_t refreshRateDenominator = 1;
        uint32_t bitsPerPixel = 32;

        [[nodiscard]]
        constexpr double refreshRate() const noexcept
        {
            if (refreshRateDenominator == 0)
            {
                return 0.0;
            }

            return static_cast<double>(refreshRateNumerator) /
                   static_cast<double>(refreshRateDenominator);
        }

        constexpr bool operator==(
            DisplayMode const&) const noexcept = default;
    };

    enum class DisplayOrientation : uint8_t
    {
        Landscape,
        Portrait,
        LandscapeFlipped,
        PortraitFlipped
    };

    struct HdrCapabilities
    {
        bool supported = false;
        float minLuminanceNits = 0.0f;
        float maxLuminanceNits = 80.0f;
        float maxFullFrameLuminanceNits = 80.0f;
    };

    struct GammaRamp
    {
        std::array<uint16_t, 256> red{};
        std::array<uint16_t, 256> green{};
        std::array<uint16_t, 256> blue{};
    };
}
