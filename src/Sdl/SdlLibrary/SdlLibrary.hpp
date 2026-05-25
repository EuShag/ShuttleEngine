#pragma once
#include <vector>
#include "../SdlKeyboard/SdlKeyMode.hpp"
#include "Sdl/SdlWindow/SdlWindow.hpp"

class SdlLibrary {
public:
	SdlLibrary();

	SdlLibrary(SdlLibrary const&) = delete;
	SdlLibrary& operator=(SdlLibrary const&) = delete;
	SdlLibrary(SdlLibrary&&) = delete;
	SdlLibrary& operator=(SdlLibrary&&) = delete;

	void setRelativeMouseMode(bool enabled);

	SdlKeyState getKeyState(SdlKeyCode keyCode) const;

	[[nodiscard]] static std::vector<char const*> getSurfaceRequiredExtensions();
	void postQuitEvent();

	bool pullEvents();

	~SdlLibrary();
};
