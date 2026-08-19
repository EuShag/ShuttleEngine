//
// Created by Shagu on 04.08.2026.
//

#include "SdlWindow.hpp"
#include <iostream>
#include <algorithm>
#include <SDL2/SDL_vulkan.h>

#ifdef _WIN32
#include <Windows.h>
#include <SDL_syswm.h>
#include <windowsx.h>
#endif

#ifdef _WIN32
LRESULT CALLBACK SdlWindow::WindowProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Получаем указатель по GWLP_USERDATA, который мы привязали в конструкторе

    auto* window = reinterpret_cast<SdlWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (uMsg == WM_NCCALCSIZE && wParam == TRUE) {
        WINDOWINFO wi = {};
        wi.cbSize = sizeof(WINDOWINFO);
        GetWindowInfo(hwnd, &wi);

        window->isMaximized = wi.dwStyle & WS_MAXIMIZE;

        if (wi.dwStyle & WS_MAXIMIZE) {
            // Если окно максимизировано, возвращаем 0.
            // Это принудительно обнуляет невидимые 8-пиксельные рамки Windows,
            // заставляя окно идеально встать в координаты (0,0) рабочей области монитора.
            return 0;
        }
    }


    if (!window) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // =========================================================================
    // ОБРАБОТКА ИЗМЕНЕНИЯ РАЗМЕРА И ОТРИСОВКИ (WM_PAINT и WM_SIZE)
    // =========================================================================
    if (uMsg == WM_PAINT) {
        if (window->paintCallback) {
            // Принудительно вызываем функцию отрисовки (drawFrame) во время блокирующего модального цикла Windows
            window->paintCallback();
        }
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0; // Сообщаем ОС, что область перерисована и валидна
    }

    if (uMsg == WM_SIZE) {
        if (window->paintCallback) {
            window->paintCallback();
        }
        // НЕ возвращаем 0, так как SDL2 должен обработать WM_SIZE для обновления своего внутреннего состояния размера!
    }
    // =========================================================================

    // 1. Корректная обработка развернутого окна (убираем сдвиг -8,-8)
    if (uMsg == WM_GETMINMAXINFO) {
        auto* mmi = reinterpret_cast<MINMAXINFO *>(lParam);
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { .cbSize = sizeof(MONITORINFO) };
        if (GetMonitorInfo(monitor, &mi)) {
            // ВАЖНО: для PTMaxPosition используй абсолютные координаты относительно экрана,
            // а не относительно начала монитора, если система многомониторная.
            mmi->ptMinTrackSize.x = 0;
            mmi->ptMinTrackSize.y = 0;

            mmi->ptMaxTrackSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxTrackSize.y = mi.rcWork.bottom - mi.rcWork.top;

            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top - 1;

            // Если окно максимизировано, ставим его ровно в левый верхний угол рабочей области
            mmi->ptMaxPosition.x = mi.rcWork.left;
            mmi->ptMaxPosition.y = mi.rcWork.top;
        }
        return 0;
    }

    // Обработка наведения мыши (Hit Test) для интеграции с Windows 11 Snap Layouts
    if (uMsg == WM_NCHITTEST) {
        int screen_x = GET_X_LPARAM(lParam);
        int screen_y = GET_Y_LPARAM(lParam);

        // Переводим экранные координаты мыши в локальные относительно нашего окна
        RECT rect;
        GetWindowRect(hwnd, &rect);
        int local_x = screen_x - rect.left;
        int local_y = screen_y - rect.top;

        // Если мышь находится в пределах высоты Title Bar (0 - 45px)
        if (local_y >= static_cast<int>(window->titlebarMinY) && local_y <= static_cast<int>(window->titlebarMaxY)) {

            // 1. Проверяем кнопку РАЗВЕРНУТЬ (Snap Layouts)
            if (local_x >= static_cast<int>(window->maximizeButtonMinX) && local_x <= static_cast<int>(window->maximizeButtonMaxX)) {
                return HTMAXBUTTON;
            }

            // 2. Для остальных кнопок (Minimize, Close) и меню File возвращаем соответствующие системные значения
            if (local_x >= static_cast<int>(window->minimizeButtonMinX) && local_x <= static_cast<int>(window->minimizeButtonMaxX)) {
                return HTMINBUTTON;
            }
            if (local_x >= static_cast<int>(window->closeButtonMinX) && local_x <= static_cast<int>(window->closeButtonMaxX)) {
                return HTCLOSE;
            }
            if (local_x >= static_cast<int>(window->menuZoneMinX) && local_x <= static_cast<int>(window->menuZoneMaxX)) {
                return HTCLIENT;
            }
        }
        // Для всех остальных областей (границы ресайза, перетаскивание)
        // отдаем управление в SDL (он сам все обработает через SdlWindowHitTest)
    }

    if (uMsg == WM_NCLBUTTONDOWN) {
        if (wParam == HTMAXBUTTON) {
            if (window->getIsMaximized()) {
                window->restore();
            } else {
                window->maximize();
            }
            return 0;
        }
        if (wParam == HTMINBUTTON) {
            window->minimize();
            return 0;
        }
        if (wParam == HTCLOSE) {
            window->close();
            return 0;
        }
    }

    // Все остальные сообщения отправляем обратно в SDL
    return CallWindowProc(window->OriginalWndProc, hwnd, uMsg, wParam, lParam);
}
#endif

// Статический метод для обработки зон перетаскивания и изменения размера через HitTest
SDL_HitTestResult SDLCALL SdlWindow::SdlWindowHitTest(SDL_Window* win, const SDL_Point* area, void* data)
{
    auto* window = static_cast<SdlWindow*>(data);

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    constexpr int border = 6;

    // 1. Зоны углов для ресайза
    if (area->y < border && area->x < border) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (area->y < border && area->x > w - border) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (area->y > h - border && area->x < border) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (area->y > h - border && area->x > w - border) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

    // 2. Стороны для ресайза
    if (area->y < border) return SDL_HITTEST_RESIZE_TOP;
    if (area->y > h - border) return SDL_HITTEST_RESIZE_BOTTOM;
    if (area->x < border) return SDL_HITTEST_RESIZE_LEFT;
    if (area->x > w - border) return SDL_HITTEST_RESIZE_RIGHT;

    // 3. Зона заголовка (Title Bar)
    if (area->y < static_cast<int>(window->titlebarMaxY) && area->y > static_cast<int>(window->titlebarMinY))
    {
        // Отдаем меню File под обработку ImGui
        if (area->x >= static_cast<int>(window->menuZoneMinX) && area->x <= static_cast<int>(window->menuZoneMaxX)) {
            return SDL_HITTEST_NORMAL;
        }

        // Отдаем зону системных кнопок под обработку ImGui (или под перехват в WM_NCHITTEST)
        if (area->x >= static_cast<int>(window->buttonZoneMinX) && area->x <= static_cast<int>(window->buttonZoneMaxX)) {
            return SDL_HITTEST_NORMAL;
        }

        // В остальном заголовок — перетаскиваемый
        return SDL_HITTEST_DRAGGABLE;
    }

    return SDL_HITTEST_NORMAL;
}

SdlWindow::SdlWindow(char const* title, int width, int height)
{
    // Эти хинты заставляют SDL2 включить тени и Aero Snap автоматически
    SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");
    SDL_SetHint("SDL_BORDERLESS_WINDOWED_STYLE", "1");

    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);

    if (!window) throw std::runtime_error("Failed to create SDL window");

    restoredWidth = width;
    restoredHeight = height;
    SDL_GetWindowPosition(window, &restoredX, &restoredY);

    SDL_SetWindowData(window, "SdlWindow", this);
    SDL_SetWindowHitTest(window, SdlWindowHitTest, this);

    // Задаем максимальные размеры, которые окно в принципе может принять
    int displayIndex = SDL_GetWindowDisplayIndex(window);
    SDL_Rect usableBounds;
    if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) == 0) {
        SDL_SetWindowMaximumSize(window, usableBounds.w, usableBounds.h);
    }

#ifdef _WIN32
    // 2. Получаем низкоуровневую информацию об окне из операционной системы
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version); // Инициализируем структуру версии SDL

    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window; // Вот наш прямой дескриптор Win32 окна!

        // Включаем системные стили, чтобы ОС понимала, что окно имеет кнопки минимизации и максимизации.
        // Без флага WS_MAXIMIZEBOX Windows 11 откажется показывать всплывающую подсказку Snap Layouts!
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        // 3. Подменяем оконную процедуру (Subclassing)
        // Сохраняем указатель на оригинальный обработчик SDL, чтобы не сломать ввод мыши/клавиатуры
        OriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProcHook)));
        // UserData
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
#endif
}

void SdlWindow::maximize() {
    if (!isMaximized) {
        SDL_GetWindowPosition(window, &restoredX, &restoredY);
        SDL_GetWindowSize(window, &restoredWidth, &restoredHeight);
    }
    SDL_MaximizeWindow(window);
    isMaximized = true;
    isMinimized = false;
}

void SdlWindow::restore() {
    SDL_RestoreWindow(window);
    // Если координаты ушли в минус из-за прилипания, корректируем их
    int const x = restoredX < 0 ? 0 : restoredX;
    int const y = restoredY < 0 ? 0 : restoredY;

    SDL_SetWindowPosition(window, x, y);
    SDL_SetWindowSize(window, restoredWidth, restoredHeight);

    isMaximized = false;
    isMinimized = false;
}

void SdlWindow::minimize() {
    SDL_MinimizeWindow(window);
    isMinimized = true;
}

vk::SurfaceKHR SdlWindow::createVulkanSurface(vk::Instance const& instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) throw std::runtime_error("Failed to create Vulkan surface");
    return surface;
}

vk::UniqueSurfaceKHR SdlWindow::createVulkanSurfaceUnique(vk::Instance const& instance) const {
    return vk::UniqueSurfaceKHR(createVulkanSurface(instance), instance);
}

void SdlWindow::getPosition(int *x, int *y) const { SDL_GetWindowPosition(window, x, y); }
void SdlWindow::processEvent(SDL_Event const& event) { dispatchEvent(event); }

vk::Extent2D SdlWindow::getExtent() const {
    int32_t width, height;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    return {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}

void SdlWindow::setPosition(int x, int y) {
    SDL_SetWindowPosition(window, x, y);
    if (!isMaximized && !isMinimized) { restoredX = x; restoredY = y; }
}

void SdlWindow::setSize(int width, int height) {
    SDL_SetWindowSize(window, width, height);
    if (!isMaximized && !isMinimized) { restoredWidth = width; restoredHeight = height; }
}

void SdlWindow::close() const {
    SDL_Event closeEvent;
    SDL_zero(closeEvent);
    closeEvent.type = SDL_WINDOWEVENT;
    closeEvent.window.event = SDL_WINDOWEVENT_CLOSE;
    closeEvent.window.windowID = SDL_GetWindowID(window);
    SDL_PushEvent(&closeEvent);
}

void SdlWindow::getRestoredPosition(int *x, int *y) const { *x = restoredX; *y = restoredY; }
void SdlWindow::getRestoredSize(int *width, int *height) const { *width = restoredWidth; *height = restoredHeight; }
void SdlWindow::getActualPosition(int *x, int *y) const { *x = actualX; *y = actualY; }
void SdlWindow::getActualSize(int *width, int *height) const { *width = actualWidth; *height = actualHeight; }

// --- Callbacks ---
void SdlWindow::setWindowCloseEventCallback(std::function<void(SdlWindow&)> cb) { hasCloseEventCallback = true; windowCloseEventCallback = std::move(cb); }
void SdlWindow::setWindowResizeEventCallback(std::function<void(SdlWindow&, int, int)> cb) { hasResizeEventCallback = true; windowResizeEventCallback = std::move(cb); }
void SdlWindow::setWindowMoveEventCallback(std::function<void(SdlWindow&, int, int)> cb) { hasMoveEventCallback = true; windowMoveEventCallback = std::move(cb); }
void SdlWindow::setWindowFocusEventCallback(std::function<void(SdlWindow&, int)> cb) { hasFocusEventCallback = true; windowFocusEventCallback = std::move(cb); }
void SdlWindow::setWindowShowModeEventCallback(std::function<void(SdlWindow&, ShowMode)> cb) { hasShowModeEventCallback = true; windowShowModeEventCallback = std::move(cb); }
void SdlWindow::setMouseMotionEventCallback(std::function<void(SdlWindow&, int, int)> cb) { hasMouseMotionEventCallback = true; mouseMotionEventCallback = std::move(cb); }
void SdlWindow::setMouseButtonEventCallback(std::function<void(SdlWindow&, SdlMouseButton, SdlKeyState)> cb) { hasMouseButtonEventCallback = true; mouseButtonEventCallback = std::move(cb); }
void SdlWindow::setMouseWheelEventCallback(std::function<void(SdlWindow&, int, int)> cb) { hasMouseWheelEventCallback = true; mouseWheelEventCallback = std::move(cb); }
void SdlWindow::setKeyboardEventCallback(std::function<void(SdlWindow&, SdlKeyCode, SdlKeyMode, SdlKeyState)> cb) { hasKeyboardEventCallback = true; keyboardEventCallback = std::move(cb); }
void SdlWindow::setWindowPaintCallback(const std::function<void()>& cb) { hasPaintCallback = true; paintCallback = std::move(cb); }

void SdlWindow::setTitlebarLayout(
    uint32_t titlebarMinY_,
    uint32_t titlebarMaxY_,
    uint32_t buttonZoneMinX_,
    uint32_t buttonZoneMaxX_,
    uint32_t dragZoneMinX_,
    uint32_t dragZoneMaxX_,
    uint32_t minimizeButtonMinX_,
    uint32_t minimizeButtonMaxX_,
    uint32_t maximizeButtonMinX_,
    uint32_t maximizeButtonMaxX_,
    uint32_t closeButtonMinX_,
    uint32_t closeButtonMaxX_,
    uint32_t menuZoneMinX_,
    uint32_t menuZoneMaxX_)
{
    this->titlebarMinY = titlebarMinY_;
    this->titlebarMaxY = titlebarMaxY_;
    this->buttonZoneMinX = buttonZoneMinX_;
    this->buttonZoneMaxX = buttonZoneMaxX_;
    this->dragZoneMinX = dragZoneMinX_;
    this->dragZoneMaxX = dragZoneMaxX_;
    this->minimizeButtonMinX = minimizeButtonMinX_;
    this->minimizeButtonMaxX = minimizeButtonMaxX_;
    this->maximizeButtonMinX = maximizeButtonMinX_;
    this->maximizeButtonMaxX = maximizeButtonMaxX_;
    this->closeButtonMinX = closeButtonMinX_;
    this->closeButtonMaxX = closeButtonMaxX_;
    this->menuZoneMinX = menuZoneMinX_;
    this->menuZoneMaxX = menuZoneMaxX_;
}

SdlWindow::~SdlWindow() { SDL_DestroyWindow(window); }

void SdlWindow::dispatchEvent(SDL_Event const& event) {
    switch (event.type) {
    case SDL_WINDOWEVENT: dispatchWindowEvent(event.window); break;
    case SDL_MOUSEMOTION: dispatchMouseMotionEvent(event.motion); break;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: dispatchMouseButtonEvent(event.button); break;
    case SDL_MOUSEWHEEL: dispatchMouseWheelEvent(event.wheel); break;
    case SDL_KEYDOWN: case SDL_KEYUP: dispatchKeyboardEvent(event.key); break;
    default: break;
    }
}

void SdlWindow::dispatchWindowEvent(SDL_WindowEvent const& e) {
    SdlWindow& win = getObjectFromId(e.windowID);
    switch (e.event) {
    case SDL_WINDOWEVENT_RESIZED:
        if (!win.isMaximized && !win.isMinimized) {
            win.restoredWidth = e.data1; win.restoredHeight = e.data2;
            win.actualWidth = e.data1; win.actualHeight = e.data2;
        }
        if (win.hasResizeEventCallback) win.windowResizeEventCallback(win, e.data1, e.data2);
        break;
    case SDL_WINDOWEVENT_MOVED:
        if (!win.isMaximized && !win.isMinimized) {
            win.restoredX = e.data1; win.restoredY = e.data2;
            win.actualX = e.data1; win.actualY = e.data2;
        }
        if (win.hasMoveEventCallback) win.windowMoveEventCallback(win, e.data1, e.data2);
        break;
    case SDL_WINDOWEVENT_MAXIMIZED: {
        win.isMaximized = true; win.isMinimized = false;
        #ifndef _WIN32
        MaximizeBorderlessWindow(win.getWindow());
        #endif
        if (win.hasShowModeEventCallback) win.windowShowModeEventCallback(win, ShowMode::Maximized);
        SDL_GetWindowPosition(win.getWindow(), &win.actualX, &win.actualY);
        SDL_GetWindowSize(win.getWindow(), &win.actualWidth, &win.actualHeight);
        break;
    }
    case SDL_WINDOWEVENT_RESTORED:
        std::cout << "Window Restored" << std::endl;
        win.isMaximized = false; win.isMinimized = false;
        if (win.hasShowModeEventCallback) win.windowShowModeEventCallback(win, ShowMode::Normal);
        win.actualWidth = win.restoredWidth; win.actualHeight = win.restoredHeight;
        win.actualX = win.restoredX; win.actualY = win.restoredY;
        break;
    case SDL_WINDOWEVENT_CLOSE:
        if (win.hasCloseEventCallback) win.windowCloseEventCallback(win);
        break;
    default: break;
    }
}

void SdlWindow::dispatchMouseMotionEvent(SDL_MouseMotionEvent const& e) { SdlWindow& w = getObjectFromId(e.windowID); if (w.hasMouseMotionEventCallback) w.mouseMotionEventCallback(w, e.x, e.y); }
void SdlWindow::dispatchMouseButtonEvent(SDL_MouseButtonEvent const& e) { if (e.windowID == 0) return; SdlWindow& w = getObjectFromId(e.windowID); if (w.hasMouseButtonEventCallback) w.mouseButtonEventCallback(w, SdlMouseButton{e.button}, SdlKeyState{e.state}); }
void SdlWindow::dispatchMouseWheelEvent(SDL_MouseWheelEvent const& e) { if (e.windowID == 0) return; SdlWindow& w = getObjectFromId(e.windowID); if (w.hasMouseWheelEventCallback) w.mouseWheelEventCallback(w, e.x, e.y); }
void SdlWindow::dispatchKeyboardEvent(SDL_KeyboardEvent const& e) { if (e.windowID == 0) return; SdlWindow& w = getObjectFromId(e.windowID); if (w.hasKeyboardEventCallback) w.keyboardEventCallback(w, SdlKeyCode{e.keysym.sym}, SdlKeyMode{e.keysym.mod}, SdlKeyState{e.state}); }

SdlWindow& SdlWindow::getObjectFromId(uint32_t id) {
    SDL_Window* sdlW = SDL_GetWindowFromID(id);
    if (!sdlW) throw std::runtime_error("Invalid window ID");
    return *static_cast<SdlWindow*>(SDL_GetWindowData(sdlW, "SdlWindow"));
}
