#pragma once
#include <functional>
#include <SDL2/SDL.h>
#include "../SdlKeyboard/SdlKeyCode.hpp"
#include "../SdlKeyboard/SdlKeyMode.hpp"
#include "../SdlMouse/SdlMouseButton.hpp"
#include "IncludeVulkan.hpp"

enum class SdlKeyState {
	Pressed = SDL_PRESSED,
	Released = SDL_RELEASED
};

enum class ShowMode {
	Normal,
	Minimized,
	Maximized,
	Fullscreen
};

class Event{};

class SdlWindow {
public:
	SdlWindow(char const* title, int width, int height);

	SDL_Window* getWindow() {
		return window;
	}

	[[nodiscard]] vk::SurfaceKHR createVulkanSurface(vk::Instance const& instance) const;

	[[nodiscard]] vk::UniqueSurfaceKHR createVulkanSurfaceUnique(vk::Instance const& instance) const;

	// Delete copy and move constructors and assignment operators
	SdlWindow(SdlWindow const&) = delete;
	SdlWindow& operator=(SdlWindow const&) = delete;
	SdlWindow(SdlWindow&&) = delete;
	SdlWindow& operator=(SdlWindow&&) = delete;

	static void processEvent(SDL_Event const& event);

	vk::Extent2D getExtent() const;

	void setPosition(int x, int y);
	void setSize(int width, int height);
	void close();
	void show();
	void hide();
	void maximize();
	void minimize();
	void restore();

	void setWindowCloseEventCallback(std::function<void(SdlWindow&)> callback);
	void setWindowResizeEventCallback(std::function<void(SdlWindow&, int, int)> callback);
	void setWindowMoveEventCallback(std::function<void(SdlWindow&, int, int)> callback);
	void setWindowFocusEventCallback(std::function<void(SdlWindow&, int)> callback);
	void setWindowShowModeEventCallback(std::function<void(SdlWindow&, ShowMode)> callback);
	void setMouseMotionEventCallback(std::function<void(SdlWindow&, int, int)> callback);
	void setMouseButtonEventCallback(std::function<void(SdlWindow&, SdlMouseButton, SdlKeyState)> callback);
	void setMouseWheelEventCallback(std::function<void(SdlWindow&, int, int)> callback);
	void setKeyboardEventCallback(std::function<void(SdlWindow&, SdlKeyCode, SdlKeyMode, SdlKeyState)> callback);

	~SdlWindow();

private:
	SDL_Window* window;

	std::function<void(SdlWindow&)> windowCloseEventCallback;

	std::function<void(SdlWindow&, int, int)> windowResizeEventCallback;
	std::function<void(SdlWindow&, int, int)> windowMoveEventCallback;
	std::function<void(SdlWindow&, int)> windowFocusEventCallback;
	std::function<void(SdlWindow&, ShowMode)> windowShowModeEventCallback;

	std::function<void(SdlWindow&, int, int)> mouseMotionEventCallback;
	std::function<void(SdlWindow&, SdlMouseButton, SdlKeyState)> mouseButtonEventCallback;
	std::function<void(SdlWindow&, int, int)> mouseWheelEventCallback;
	std::function<void(SdlWindow&, SdlKeyCode, SdlKeyMode, SdlKeyState)> keyboardEventCallback;

	bool hasCloseEventCallback = false;
	bool hasResizeEventCallback = false;
	bool hasMoveEventCallback = false;
	bool hasFocusEventCallback = false;
	bool hasShowModeEventCallback = false;
	bool hasMouseMotionEventCallback = false;
	bool hasMouseButtonEventCallback = false;
	bool hasMouseWheelEventCallback = false;
	bool hasKeyboardEventCallback = false;

	static void dispatchEvent(SDL_Event const& event);

	static void dispatchWindowEvent(SDL_WindowEvent const& windowEvent);
	static void dispatchMouseMotionEvent(SDL_MouseMotionEvent const& mouseMotionEvent);
	static void dispatchMouseButtonEvent(SDL_MouseButtonEvent const& mouseButtonEvent);
	static void dispatchMouseWheelEvent(SDL_MouseWheelEvent const& mouseWheelEvent);
	static void dispatchKeyboardEvent(SDL_KeyboardEvent const& keyboardEvent);

	static SdlWindow& getObjectFromId(uint32_t windowId);
};