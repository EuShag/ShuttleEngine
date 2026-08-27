#pragma once

#include <cstdint>
#include <initializer_list>

namespace shuttle::pal
{
    // Используем типизированный enum для каждого бита
    enum class KeyModeBit : uint16_t
    {
        None    = 0,
        LShift  = 1 << 0,
        RShift  = 1 << 1,
        LCtrl   = 1 << 2,
        RCtrl   = 1 << 3,
        LAlt    = 1 << 4,
        RAlt    = 1 << 5,
        LGUI    = 1 << 6, // Win / Command
        RGUI    = 1 << 7,
        Num     = 1 << 8,
        Caps    = 1 << 9,
        Scroll  = 1 << 10,

        // Комбинированные маски (удобство)
        Ctrl    = LCtrl | RCtrl,
        Shift   = LShift | RShift,
        Alt     = LAlt | RAlt,
        GUI     = LGUI | RGUI
    };

    class KeyMode
    {
    public:
        constexpr KeyMode() = default;
        constexpr KeyMode(KeyModeBit bit) : m_bits(static_cast<uint16_t>(bit)) {}
        constexpr KeyMode(std::initializer_list<KeyModeBit> bits)
        {
            for (auto bit : bits) m_bits |= static_cast<uint16_t>(bit);
        }
        explicit constexpr KeyMode(uint16_t bits) : m_bits(bits) {}

        // Проверка: установлен ли хотя бы один бит из маски?
        [[nodiscard]] constexpr bool hasAny(KeyModeBit bit) const noexcept
        {
            return (m_bits & static_cast<uint16_t>(bit)) != 0;
        }

        // Проверка: установлены ли ВСЕ биты из маски?
        [[nodiscard]] constexpr bool hasAll(KeyModeBit bit) const noexcept
        {
            return (m_bits & static_cast<uint16_t>(bit)) == static_cast<uint16_t>(bit);
        }

        // Операторы для удобного комбинирования
        constexpr KeyMode& operator|=(KeyModeBit bit) noexcept
        {
            m_bits |= static_cast<uint16_t>(bit);
            return *this;
        }

        [[nodiscard]] constexpr uint16_t getRaw() const noexcept { return m_bits; }

        bool operator|(KeyModeBit l_shift) const {
            return static_cast<uint16_t>(m_bits) | static_cast<uint16_t>(l_shift);
        }

        bool operator&(KeyModeBit l_shift) const {
            return static_cast<uint16_t>(m_bits) & static_cast<uint16_t>(l_shift);
        }

    private:
        uint16_t m_bits = 0;
    };
}
