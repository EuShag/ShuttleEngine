#pragma once
#include <algorithm>
#include <ranges>

enum class SdlKeyModeBits
{
    None = KMOD_NONE,
    LShift = KMOD_LSHIFT,
    RShift = KMOD_RSHIFT,
    LCtrl = KMOD_LCTRL,
    RCtrl = KMOD_RCTRL,
    LAlt = KMOD_LALT,
    RAlt = KMOD_RALT,
    LGUI = KMOD_LGUI,
    RGUI = KMOD_RGUI,
    Num = KMOD_NUM,
    Caps = KMOD_CAPS,
    Mode = KMOD_MODE,
    Scroll = KMOD_SCROLL,

    Ctrl = KMOD_CTRL,
    Shift = KMOD_SHIFT,
    Alt = KMOD_ALT,
    GUI = KMOD_GUI,

    Reserved = KMOD_RESERVED
};

class SdlKeyMode {
public:
	SdlKeyMode() = default;
	explicit SdlKeyMode(Uint16 keyModeBits) : keyModeBits(keyModeBits) {}
    bool operator||(SdlKeyModeBits bit) const {
		return (keyModeBits & static_cast<uint32_t>(bit)) != 0;
    }
	bool operator&&(SdlKeyModeBits bit) const {
        return (keyModeBits & static_cast<uint32_t>(bit)) == static_cast<uint32_t>(bit);
	}
	bool operator||(std::initializer_list<SdlKeyModeBits> bits) const {
        return std::ranges::all_of(bits, [this](SdlKeyModeBits bit) {
            return (keyModeBits & static_cast<uint32_t>(bit)) != 0;
			});
    }
    bool operator&&(std::initializer_list<SdlKeyModeBits> bits) const {
        return std::ranges::all_of(bits, [this](SdlKeyModeBits bit) {
			return (keyModeBits & static_cast<uint32_t>(bit)) == static_cast<uint32_t>(bit);
			});
	}
private:
	Uint16 keyModeBits = 0;
};