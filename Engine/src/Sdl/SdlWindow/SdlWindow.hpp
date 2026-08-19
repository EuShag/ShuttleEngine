#pragma once
#include <functional>
#include <SDL2/SDL.h>
#include "../SdlKeyboard/SdlKeyCode.hpp"
#include "../SdlKeyboard/SdlKeyMode.hpp"
#include "../SdlMouse/SdlMouseButton.hpp"
#include "IncludeVulkan.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

enum class SdlKeyState
{
Pressed = SDL_PRESSED,
Released = SDL_RELEASED
};

enum class ShowMode
{
    Normal,
    Minimized,
    Maximized,
    Fullscreen
};

class SdlWindow
{
public:
    SdlWindow(char const* title, int width, int height);

    SDL_Window* getWindow() { return window; }

    [[nodiscard]] vk::SurfaceKHR createVulkanSurface(vk::Instance const& instance) const;

    [[nodiscard]] vk::UniqueSurfaceKHR createVulkanSurfaceUnique(vk::Instance const& instance) const;

    void getPosition(int * x, int * y) const;

    // Delete copy and move constructors and assignment operators
    SdlWindow(SdlWindow const&) = delete;
    SdlWindow& operator=(SdlWindow const&) = delete;
    SdlWindow(SdlWindow&&) = delete;
    SdlWindow& operator=(SdlWindow&&) = delete;

    static void processEvent(SDL_Event const& event);

    [[nodiscard]] vk::Extent2D getExtent() const;

    void setPosition(int x, int y);
    void setSize(int width, int height);
    void close() const;
    void show();
    void hide();
    void maximize();
    void minimize();
    void restore();

    void getRestoredPosition(int * x, int * y) const;
    void getRestoredSize(int * width, int * height) const;

    void getActualPosition(int * x, int * y) const;
    void getActualSize(int * width, int * height) const;

    void setWindowCloseEventCallback(std::function<void(SdlWindow&)> callback);
    void setWindowResizeEventCallback(std::function<void(SdlWindow&, int, int)> callback);
    void setWindowMoveEventCallback(std::function<void(SdlWindow&, int, int)> callback);
    void setWindowFocusEventCallback(std::function<void(SdlWindow&, int)> callback);
    void setWindowShowModeEventCallback(std::function<void(SdlWindow&, ShowMode)> callback);
    void setMouseMotionEventCallback(std::function<void(SdlWindow&, int, int)> callback);
    void setMouseButtonEventCallback(std::function<void(SdlWindow&, SdlMouseButton, SdlKeyState)> callback);
    void setMouseWheelEventCallback(std::function<void(SdlWindow&, int, int)> callback);
    void setKeyboardEventCallback(std::function<void(SdlWindow&, SdlKeyCode, SdlKeyMode, SdlKeyState)> callback);

    void setWindowPaintCallback(const std::function<void()>& cb);

    void setTitlebarLayout(
    uint32_t titlebarMinY_ = 0,
    uint32_t titlebarMaxY_ = 0,
    uint32_t buttonZoneMinX_ = 0,
    uint32_t buttonZoneMaxX_ = 0,

    uint32_t dragZoneMinX_ = 0,
    uint32_t dragZoneMaxX_ = 0,

    uint32_t minimizeButtonMinX_ = 0,
    uint32_t minimizeButtonMaxX_ = 0,
    uint32_t maximizeButtonMinX_ = 0,
    uint32_t maximizeButtonMaxX_ = 0,
    uint32_t closeButtonMinX_ = 0,
    uint32_t closeButtonMaxX_ = 0,

    uint32_t menuZoneMinX_ = 0,
    uint32_t menuZoneMaxX_ = 0
    );

    [[nodiscard]] bool getIsMaximized() const{ return isMaximized; }
    [[nodiscard]] bool getIsMinimized() const{ return isMinimized; }

    ~SdlWindow();

private:
    SDL_Window* window;

    std::function<void(SdlWindow&)> windowCloseEventCallback;

    std::function<void(SdlWindow&, int, int)> windowResizeEventCallback;
    std::function<void(SdlWindow&, int, int)> windowMoveEventCallback;
    std::function<void(SdlWindow&, int)> windowFocusEventCallback;
    std::function<void(SdlWindow&, ShowMode)> windowShowModeEventCallback;
    std::function<void()> paintCallback;

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
    bool hasPaintCallback = false;

    int32_t restoredWidth = 0;
    int32_t restoredHeight = 0;
    int32_t restoredX = 0;
    int32_t restoredY = 0;
    bool isMaximized = false;
    bool isMinimized = false;

    uint32_t titlebarMinY = 0;
    uint32_t titlebarMaxY = 0;
    uint32_t buttonZoneMinX = 0;
    uint32_t buttonZoneMaxX = 0;

    uint32_t dragZoneMinX = 0;
    uint32_t dragZoneMaxX = 0;

    uint32_t minimizeButtonMinX = 0;
    uint32_t minimizeButtonMaxX = 0;
    uint32_t maximizeButtonMinX = 0;
    uint32_t maximizeButtonMaxX = 0;
    uint32_t closeButtonMinX = 0;
    uint32_t closeButtonMaxX = 0;

    uint32_t menuZoneMinX = 0;
    uint32_t menuZoneMaxX = 0;

    int32_t actualWidth = 0;
    int32_t actualHeight = 0;
    int32_t actualX = 0;
    int32_t actualY = 0;

    static void dispatchEvent(SDL_Event const& event);

    static void dispatchWindowEvent(SDL_WindowEvent const& windowEvent);
    static void dispatchMouseMotionEvent(SDL_MouseMotionEvent const& mouseMotionEvent);
    static void dispatchMouseButtonEvent(SDL_MouseButtonEvent const& mouseButtonEvent);
    static void dispatchMouseWheelEvent(SDL_MouseWheelEvent const& mouseWheelEvent);
    static void dispatchKeyboardEvent(SDL_KeyboardEvent const& keyboardEvent);

    static SdlWindow& getObjectFromId(uint32_t windowId);

public:
    void setResizeBeginCallback(std::function<void()> callback) { resizeBeginCallback = std::move(callback); }
    void setResizeEndCallback(std::function<void()> callback) { resizeEndCallback = std::move(callback); }

private:
    std::function<void()> resizeBeginCallback;
    std::function<void()> resizeEndCallback;


#ifdef _WIN32
    static LRESULT CALLBACK WindowProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static SDL_HitTestResult SDLCALL SdlWindowHitTest(SDL_Window* win, const SDL_Point* area, void* data);
    WNDPROC OriginalWndProc = nullptr;
#endif
};