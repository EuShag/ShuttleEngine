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

	void addCustomEventProcessor(std::function<void (SDL_Event const& event)> const &processor);

	bool pullEvents();

	~SdlLibrary();
private:
	std::vector<std::function<void (SDL_Event const& event)>> customEventProcessors;
};
