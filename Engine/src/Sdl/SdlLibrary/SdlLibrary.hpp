#pragma once
#include <vector>
#include "../SdlKeyboard/SdlKeyMode.hpp"
#include "../SdlKeyboard/SdlKeyCode.hpp"
#include "Sdl/SdlWindow/SdlWindow.hpp"

class SdlLibrary
{
  public:
    SdlLibrary();

    SdlLibrary(SdlLibrary const&) = delete;
    SdlLibrary& operator=(SdlLibrary const&) = delete;
    SdlLibrary(SdlLibrary&&) = delete;
    SdlLibrary& operator=(SdlLibrary&&) = delete;

    static void setRelativeMouseMode(bool enabled);

    [[nodiscard]] SdlKeyState getKeyState(SdlKeyCode keyCode) const;

    [[nodiscard]] static std::vector<char const*> getSurfaceRequiredExtensions();
    void postQuitEvent();

    void addCustomEventProcessor(std::function<void(SDL_Event const& event)> const& processor);
    void setCurrentWindow(SdlWindow* window) {
        currentWindow = window;
    }

    [[nodiscard]] bool pullEvents() const;

    ~SdlLibrary();

  private:
    std::vector<std::function<void(SDL_Event const& event)>> customEventProcessors;
    SdlWindow* currentWindow = nullptr;
};
