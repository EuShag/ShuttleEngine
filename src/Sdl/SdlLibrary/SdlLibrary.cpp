#include "SdlLibrary.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "../SdlWindow/SdlWindow.hpp"

#include <stdexcept>

SdlLibrary::SdlLibrary() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		throw std::runtime_error("Failed to initialize SDL");
	}
}

void SdlLibrary::setRelativeMouseMode(bool enabled)
{
	SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
}

SdlKeyState SdlLibrary::getKeyState(SdlKeyCode keyCode) const {
	int keyCount = 0;
	auto keyState = SDL_GetKeyboardState(&keyCount);
	if (keyState == nullptr) {}
}

[[nodiscard]] std::vector<char const*> SdlLibrary::getSurfaceRequiredExtensions() {
	uint32_t sdlExtensionCount = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtensionCount, nullptr)) {
		throw std::runtime_error("Failed to get SDL Vulkan extension count");
	}
	std::vector<char const*> extensions(sdlExtensionCount);
	if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlExtensionCount, extensions.data())) {
		throw std::runtime_error("Failed to get SDL Vulkan extensions");
	}
	return extensions;
}

void SdlLibrary::postQuitEvent() {
	SDL_Event quitEvent;
	quitEvent.type = SDL_QUIT;
	SDL_PushEvent(&quitEvent);
}

void SdlLibrary::addCustomEventProcessor(std::function<void(SDL_Event const& event)> const &processor) {
	if (processor == nullptr) {
		return;
	}
	customEventProcessors.push_back(processor);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
bool SdlLibrary::pullEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) return false;
		for (auto customEventProcessor : customEventProcessors) {
			customEventProcessor(event);
		}
		SdlWindow::processEvent(event);
	}
	return true;
}

SdlLibrary::~SdlLibrary() {
	SDL_Quit();
}
