#include "SdlWindow.hpp"
#include <SDL2/SDL_vulkan.h>

SdlWindow::SdlWindow(char const* title, int width, int height)
{
    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);

    if (!window)
    {
        throw std::runtime_error("Failed to create SDL window");
    }
    SDL_SetWindowData(window, "SdlWindow", this);
}

[[nodiscard]] vk::SurfaceKHR SdlWindow::createVulkanSurface(vk::Instance const& instance) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
    {
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    return surface;
}

[[nodiscard]] vk::UniqueSurfaceKHR SdlWindow::createVulkanSurfaceUnique(vk::Instance const& instance) const
{
    return vk::UniqueSurfaceKHR(createVulkanSurface(instance), instance);
}

void SdlWindow::processEvent(SDL_Event const& event)
{
    dispatchEvent(event);
}

vk::Extent2D SdlWindow::getExtent() const
{
    int32_t width, height;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    return {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}

void SdlWindow::setWindowCloseEventCallback(std::function<void(SdlWindow&)> callback)
{
    hasCloseEventCallback = true;
    windowCloseEventCallback = std::move(callback);
}

void SdlWindow::setWindowResizeEventCallback(std::function<void(SdlWindow&, int, int)> callback)
{
    hasResizeEventCallback = true;
    windowResizeEventCallback = std::move(callback);
}

void SdlWindow::setWindowMoveEventCallback(std::function<void(SdlWindow&, int, int)> callback)
{
    hasMoveEventCallback = true;
    windowMoveEventCallback = std::move(callback);
}

void SdlWindow::setWindowFocusEventCallback(std::function<void(SdlWindow&, int)> callback)
{
    hasFocusEventCallback = true;
    windowFocusEventCallback = std::move(callback);
}

void SdlWindow::setWindowShowModeEventCallback(std::function<void(SdlWindow&, ShowMode)> callback)
{
    hasShowModeEventCallback = true;
    windowShowModeEventCallback = std::move(callback);
}

void SdlWindow::setMouseMotionEventCallback(std::function<void(SdlWindow&, int, int)> callback)
{
    hasMouseMotionEventCallback = true;
    mouseMotionEventCallback = std::move(callback);
}

void SdlWindow::setMouseButtonEventCallback(std::function<void(SdlWindow&, SdlMouseButton, SdlKeyState)> callback)
{
    hasMouseButtonEventCallback = true;
    mouseButtonEventCallback = std::move(callback);
}

void SdlWindow::setMouseWheelEventCallback(std::function<void(SdlWindow&, int, int)> callback)
{
    hasMouseWheelEventCallback = true;
    mouseWheelEventCallback = std::move(callback);
}

void SdlWindow::setKeyboardEventCallback(std::function<void(SdlWindow&, SdlKeyCode, SdlKeyMode, SdlKeyState)> callback)
{
    hasKeyboardEventCallback = true;
    keyboardEventCallback = std::move(callback);
}

SdlWindow::~SdlWindow()
{
    SDL_DestroyWindow(window);
}

void SdlWindow::dispatchEvent(SDL_Event const& event)
{
    switch (event.type)
    {
    case SDL_WINDOWEVENT: dispatchWindowEvent(event.window); break;
    case SDL_MOUSEMOTION: dispatchMouseMotionEvent(event.motion); break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: dispatchMouseButtonEvent(event.button); break;
    case SDL_MOUSEWHEEL: dispatchMouseWheelEvent(event.wheel); break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: dispatchKeyboardEvent(event.key); break;
    default: break;
    }
}

void SdlWindow::dispatchWindowEvent(SDL_WindowEvent const& windowEvent)
{
    SdlWindow& window = getObjectFromId(windowEvent.windowID);
    switch (windowEvent.event)
    {
    case SDL_WINDOWEVENT_RESIZED:
        if (window.hasResizeEventCallback)
        {
            window.windowResizeEventCallback(window, windowEvent.data1, windowEvent.data2);
        }
        break;
    case SDL_WINDOWEVENT_MOVED:
        if (window.hasMoveEventCallback)
        {
            window.windowMoveEventCallback(window, windowEvent.data1, windowEvent.data2);
        }
        break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
    case SDL_WINDOWEVENT_FOCUS_LOST:
        if (window.hasFocusEventCallback)
        {
            window.windowFocusEventCallback(window, windowEvent.event == SDL_WINDOWEVENT_FOCUS_GAINED);
        }
        break;
    case SDL_WINDOWEVENT_SHOWN:
        if (window.hasShowModeEventCallback)
        {
            window.windowShowModeEventCallback(window, ShowMode::Normal);
        }
        break;
    case SDL_WINDOWEVENT_HIDDEN:
        if (window.hasShowModeEventCallback)
        {
            window.windowShowModeEventCallback(window, ShowMode::Minimized);
        }
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        if (window.hasShowModeEventCallback)
        {
            window.windowShowModeEventCallback(window, ShowMode::Maximized);
        }
        break;
    case SDL_WINDOWEVENT_MINIMIZED:
        if (window.hasShowModeEventCallback)
        {
            window.windowShowModeEventCallback(window, ShowMode::Minimized);
        }
        break;
    case SDL_WINDOWEVENT_RESTORED:
        if (window.hasShowModeEventCallback)
        {
            window.windowShowModeEventCallback(window, ShowMode::Normal);
        }
        break;
    case SDL_WINDOWEVENT_CLOSE:
        if (window.hasCloseEventCallback)
        {
            window.windowCloseEventCallback(window);
        }
        break;
    default: break;
    }
}

void SdlWindow::dispatchMouseMotionEvent(SDL_MouseMotionEvent const& mouseMotionEvent)
{
    SdlWindow& window = getObjectFromId(mouseMotionEvent.windowID);
    if (window.hasMouseMotionEventCallback)
    {
        window.mouseMotionEventCallback(window, mouseMotionEvent.x, mouseMotionEvent.y);
    }
}

void SdlWindow::dispatchMouseButtonEvent(SDL_MouseButtonEvent const& mouseButtonEvent)
{
    SdlWindow& window = getObjectFromId(mouseButtonEvent.windowID);
    if (window.hasMouseButtonEventCallback)
    {
        window.mouseButtonEventCallback(window, SdlMouseButton{mouseButtonEvent.button},
                                        SdlKeyState{mouseButtonEvent.state});
    }
}

void SdlWindow::dispatchMouseWheelEvent(SDL_MouseWheelEvent const& mouseWheelEvent)
{
    SdlWindow& window = getObjectFromId(mouseWheelEvent.windowID);
    if (window.hasMouseWheelEventCallback)
    {
        window.mouseWheelEventCallback(window, mouseWheelEvent.x, mouseWheelEvent.y);
    }
}

void SdlWindow::dispatchKeyboardEvent(SDL_KeyboardEvent const& keyboardEvent)
{
    SdlWindow& window = getObjectFromId(keyboardEvent.windowID);
    if (window.hasKeyboardEventCallback)
    {
        window.keyboardEventCallback(window, SdlKeyCode{keyboardEvent.keysym.sym}, SdlKeyMode{keyboardEvent.keysym.mod},
                                     SdlKeyState{keyboardEvent.state});
    }
}

SdlWindow& SdlWindow::getObjectFromId(uint32_t windowId)
{
    SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
    return *static_cast<SdlWindow*>(SDL_GetWindowData(sdlWindow, "SdlWindow"));
}