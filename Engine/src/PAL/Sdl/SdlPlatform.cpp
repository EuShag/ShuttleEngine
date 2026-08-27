#if !defined(_WIN32) || defined(SHUTTLE_FORCE_SDL)
#include "SdlPlatform.hpp"
#include "PAL/Common/Window/WindowBase.hpp"
#include "PAL/Common/Events/Events.hpp"
#include <backends/imgui_impl_sdl2.h>

#include <vulkan/Vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
    // =========================================================================
    // КОНВЕРТАТОРЫ СИГНАЛОВ SDL -> SHUTTLE ENGINE
    // =========================================================================

    shuttle::pal::KeyCode mapSdlKey(SDL_Keycode sym) noexcept
    {
        using shuttle::pal::KeyCode;
        switch (sym)
        {
            case SDLK_RETURN:    return KeyCode::Return;
            case SDLK_ESCAPE:    return KeyCode::Escape;
            case SDLK_BACKSPACE: return KeyCode::Backspace;
            case SDLK_TAB:       return KeyCode::Tab;
            case SDLK_SPACE:     return KeyCode::Space;
            case SDLK_CAPSLOCK:  return KeyCode::CapsLock;

            case SDLK_0:         return KeyCode::Num0;
            case SDLK_1:         return KeyCode::Num1;
            case SDLK_2:         return KeyCode::Num2;
            case SDLK_3:         return KeyCode::Num3;
            case SDLK_4:         return KeyCode::Num4;
            case SDLK_5:         return KeyCode::Num5;
            case SDLK_6:         return KeyCode::Num6;
            case SDLK_7:         return KeyCode::Num7;
            case SDLK_8:         return KeyCode::Num8;
            case SDLK_9:         return KeyCode::Num9;

            case SDLK_a:         return KeyCode::A;
            case SDLK_b:         return KeyCode::B;
            case SDLK_c:         return KeyCode::C;
            case SDLK_d:         return KeyCode::D;
            case SDLK_e:         return KeyCode::E;
            case SDLK_f:         return KeyCode::F;
            case SDLK_g:         return KeyCode::G;
            case SDLK_h:         return KeyCode::H;
            case SDLK_i:         return KeyCode::I;
            case SDLK_j:         return KeyCode::J;
            case SDLK_k:         return KeyCode::K;
            case SDLK_l:         return KeyCode::L;
            case SDLK_m:         return KeyCode::M;
            case SDLK_n:         return KeyCode::N;
            case SDLK_o:         return KeyCode::O;
            case SDLK_p:         return KeyCode::P;
            case SDLK_q:         return KeyCode::Q;
            case SDLK_r:         return KeyCode::R;
            case SDLK_s:         return KeyCode::S;
            case SDLK_t:         return KeyCode::T;
            case SDLK_u:         return KeyCode::U;
            case SDLK_v:         return KeyCode::V;
            case SDLK_w:         return KeyCode::W;
            case SDLK_x:         return KeyCode::X;
            case SDLK_y:         return KeyCode::Y;
            case SDLK_z:         return KeyCode::Z;

            case SDLK_F1:        return KeyCode::F1;
            case SDLK_F2:        return KeyCode::F2;
            case SDLK_F3:        return KeyCode::F3;
            case SDLK_F4:        return KeyCode::F4;
            case SDLK_F5:        return KeyCode::F5;
            case SDLK_F6:        return KeyCode::F6;
            case SDLK_F7:        return KeyCode::F7;
            case SDLK_F8:        return KeyCode::F8;
            case SDLK_F9:        return KeyCode::F9;
            case SDLK_F10:       return KeyCode::F10;
            case SDLK_F11:       return KeyCode::F11;
            case SDLK_F12:       return KeyCode::F12;

            case SDLK_PRINTSCREEN: return KeyCode::PrintScreen;
            case SDLK_SCROLLLOCK:  return KeyCode::ScrollLock;
            case SDLK_PAUSE:       return KeyCode::Pause;
            case SDLK_INSERT:      return KeyCode::Insert;
            case SDLK_HOME:        return KeyCode::Home;
            case SDLK_PAGEUP:      return KeyCode::PageUp;
            case SDLK_PAGEDOWN:    return KeyCode::PageDown;
            case SDLK_DELETE:      return KeyCode::Delete;
            case SDLK_END:         return KeyCode::End;

            case SDLK_RIGHT:     return KeyCode::Right;
            case SDLK_LEFT:      return KeyCode::Left;
            case SDLK_DOWN:      return KeyCode::Down;
            case SDLK_UP:        return KeyCode::Up;

            case SDLK_LCTRL:     return KeyCode::LCtrl;
            case SDLK_RCTRL:     return KeyCode::RCtrl;
            case SDLK_LSHIFT:    return KeyCode::LShift;
            case SDLK_RSHIFT:    return KeyCode::RShift;
            case SDLK_LALT:      return KeyCode::LAlt;
            case SDLK_RALT:      return KeyCode::RAlt;
            case SDLK_LGUI:      return KeyCode::LGUI;
            case SDLK_RGUI:      return KeyCode::RGUI;

            default:             return KeyCode::Unknown;
        }
    }

    [[nodiscard]] KeyMode getSdlKeyMode(SDL_Keymod sdl_mod_state = SDL_KMOD_NONE) noexcept
    {
        // Если маска не передана из события, запрашиваем актуальное состояние у SDL
        if (sdl_mod_state == SDL_KMOD_NONE)
        {
            sdl_mod_state = SDL_GetModState();
        }

        // Создаем пустой объект KeyMode (использует constexpr конструктор по умолчанию)
        KeyMode mode;

        // Используем ваш перегруженный оператор |= для KeyModeBit
        if (sdl_mod_state & SDL_KMOD_LSHIFT) mode |= KeyModeBit::LShift;
        if (sdl_mod_state & SDL_KMOD_RSHIFT) mode |= KeyModeBit::RShift;
        if (sdl_mod_state & SDL_KMOD_LCTRL)  mode |= KeyModeBit::LCtrl;
        if (sdl_mod_state & SDL_KMOD_RCTRL)  mode |= KeyModeBit::RCtrl;
        if (sdl_mod_state & SDL_KMOD_LALT)   mode |= KeyModeBit::LAlt;
        if (sdl_mod_state & SDL_KMOD_RALT)   mode |= KeyModeBit::RAlt;
        if (sdl_mod_state & SDL_KMOD_LGUI)   mode |= KeyModeBit::LGUI;
        if (sdl_mod_state & SDL_KMOD_RGUI)   mode |= KeyModeBit::RGUI;

        // Фиксирующие клавиши (Caps, Num, Scroll)
        if (sdl_mod_state & SDL_KMOD_NUM)    mode |= KeyModeBit::Num;
        if (sdl_mod_state & SDL_KMOD_CAPS)   mode |= KeyModeBit::Caps;
        if (sdl_mod_state & SDL_KMOD_SCROLL) mode |= KeyModeBit::Scroll;

        return mode;
    }

    // Функция-наблюдатель событий SDL2
    int SDLCALL WindowEventWatcher(void* userdata, SDL_Event* event) {
        // Проверяем, что событие относится к изменению размера окна
        if (event->type == SDL_WINDOWEVENT &&
           (event->window.event == SDL_WINDOWEVENT_RESIZED ||
            event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {

                SDL_Window* sdlWin = SDL_GetWindowFromID(event->window.windowID);
                auto* window = reinterpret_cast<shuttle::pal::WindowBase*>(SDL_GetWindowData(sdlWin, "WindowBase"));

                int width = event->window.data1;
                int height = event->window.data2;

                // 1. Обновляем размер вашего Vulkan Swapchain под новые width и height
                // ResizeVulkanSwapchain(width, height);

                // 2. Принудительно вызываем функцию отрисовки кадра
                if (window) {
                    auto listener = window->getWindowListener();
                    if (listener) {
                        listener->onWindowResize({ static_cast<uint32_t>(width), static_cast<uint32_t>(height) });
                        listener->onWindowPaint(); // Вызываем перерисовку окна
                    }
                }
            }
        return 0; // Возвращаем 0, чтобы событие пошло дальше в общую очередь
    }

    // =========================================================================
    // КРОСС-ПЛАТФОРМЕННЫЙ HIT-TEST ДЛЯ ДЕКОРАЦИЙ В SDL2
    // =========================================================================
    SDL_HitTestResult SDLCALL sdlHitTestCallback(SDL_Window* win, const SDL_Point* area, void* data)
    {
        auto* window = reinterpret_cast<shuttle::pal::WindowBase*>(SDL_GetWindowData(win, "WindowBase"));
        if (!window)
        {
            return SDL_HITTEST_NORMAL;
        }

        // Вызываем твой единый алгоритм декораций
        shuttle::pal::WindowHitTestResult result = window->getDecorations().evaluate(
            area->x, area->y,
            static_cast<int32_t>(window->getWidth()),
            static_cast<int32_t>(window->getHeight()),
            window->isMaximized()
        );

        switch (result)
        {
            case shuttle::pal::WindowHitTestResult::Client:            return SDL_HITTEST_NORMAL;
            case shuttle::pal::WindowHitTestResult::Caption:           return SDL_HITTEST_DRAGGABLE;
            case shuttle::pal::WindowHitTestResult::ResizeTop:         return SDL_HITTEST_RESIZE_TOP;
            case shuttle::pal::WindowHitTestResult::ResizeBottom:      return SDL_HITTEST_RESIZE_BOTTOM;
            case shuttle::pal::WindowHitTestResult::ResizeLeft:        return SDL_HITTEST_RESIZE_LEFT;
            case shuttle::pal::WindowHitTestResult::ResizeRight:       return SDL_HITTEST_RESIZE_RIGHT;
            case shuttle::pal::WindowHitTestResult::ResizeTopLeft:     return SDL_HITTEST_RESIZE_TOPLEFT;
            case shuttle::pal::WindowHitTestResult::ResizeTopRight:    return SDL_HITTEST_RESIZE_TOPRIGHT;
            case shuttle::pal::WindowHitTestResult::ResizeBottomLeft:  return SDL_HITTEST_RESIZE_BOTTOMLEFT;
            case shuttle::pal::WindowHitTestResult::ResizeBottomRight: return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

            // Кнопки (Close, Min, Max) возвращают NORMAL, чтобы SDL пропускал события клика мыши в клиентскую зону,
            // где твой UI (RmlUi/ImGui) сможет сам обработать нажатие кнопки.
            case shuttle::pal::WindowHitTestResult::MinimizeButton:
            case shuttle::pal::WindowHitTestResult::MaximizeButton:
            case shuttle::pal::WindowHitTestResult::CloseButton:
            case shuttle::pal::WindowHitTestResult::SystemMenu:
                return SDL_HITTEST_NORMAL;

            default: return SDL_HITTEST_NORMAL;
        }
    }

    shuttle::pal::MonitorInfo populateSdlMonitorData(int displayIndex)
    {
        shuttle::pal::MonitorInfo info{};
        info.handle = { static_cast<uintptr_t>(displayIndex) };
        info.name = SDL_GetDisplayName(displayIndex);
        info.deviceName = info.name;
        info.isPrimary = (displayIndex == 0);

        SDL_Rect rect;
        if (SDL_GetDisplayBounds(displayIndex, &rect) == 0)
        {
            info.bounds = { rect.x, rect.y, rect.w, rect.h };
        }

        if (SDL_GetDisplayUsableBounds(displayIndex, &rect) == 0)
        {
            info.workArea = { rect.x, rect.y, rect.w, rect.h };
        }

        float ddpi, hdpi, vdpi;
        if (SDL_GetDisplayDPI(displayIndex, &ddpi, &hdpi, &vdpi) == 0)
        {
            info.dpiScale = hdpi / 96.0f;
        }
        else
        {
            info.dpiScale = 1.0f;
        }

        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(displayIndex, &dm) == 0)
        {
            info.currentMode = {
                static_cast<uint32_t>(dm.w),
                static_cast<uint32_t>(dm.h),
                static_cast<uint32_t>(dm.refresh_rate),
                1, 32
            };
        }

        return info;
    }
}

namespace shuttle::pal::impl
{
    // =========================================================================
    // SdlPlatform: ИНИЦИАЛИЗАЦИЯ И ЖИЗНЕННЫЙ ЦИКЛ
    // =========================================================================

    SdlPlatform::SdlPlatform()
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
        {
            throw std::runtime_error(std::string("SdlPlatform: Failed to init SDL! Error: ") + SDL_GetError());
        }

        m_startCounter = SDL_GetPerformanceCounter();
        m_counterFrequency = SDL_GetPerformanceFrequency();
        m_initialized = true;
    }

    SdlPlatform::~SdlPlatform()
    {
        SDL_Quit();
    }

    bool SdlPlatform::pollEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // Передаем событие в ImGui
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT)
            {
                m_shouldQuit = true;
                return false;
            }

            // Маршрутизация событий окон
            if (event.type == SDL_WINDOWEVENT)
            {
                SDL_Window* sdlWin = SDL_GetWindowFromID(event.window.windowID);
                if (sdlWin)
                {
                    auto* window = reinterpret_cast<WindowBase*>(SDL_GetWindowData(sdlWin, "WindowBase"));
                    if (window)
                    {
                        auto* listener = window->getWindowListener();

                        switch (event.window.event)
                        {
                            case SDL_WINDOWEVENT_RESIZED:
                            {
                                uint32_t w = event.window.data1;
                                uint32_t h = event.window.data2;
                                window->internalOnResize(w, h);
                                if (listener) {
                                    listener->onWindowResize({ w, h });
                                    listener->onWindowPaint();
                                }
                                break;
                            }
                            case SDL_WINDOWEVENT_EXPOSED :
                            {
                                if (listener) listener->onWindowPaint();
                                break;
                            }
                            case SDL_WINDOWEVENT_MOVED:
                            {
                                int32_t x = event.window.data1;
                                int32_t y = event.window.data2;
                                window->internalOnMove(x, y);
                                if (listener) listener->onWindowMove({ x, y });
                                break;
                            }
                            case SDL_WINDOWEVENT_FOCUS_GAINED:
                            {
                                window->internalOnFocus(true);
                                if (listener) listener->onWindowFocusChanged({ true });
                                break;
                            }
                            case SDL_WINDOWEVENT_FOCUS_LOST:
                            {
                                window->internalOnFocus(false);
                                if (listener) listener->onWindowFocusChanged({ false });
                                break;
                            }
                            case SDL_WINDOWEVENT_MAXIMIZED:
                            {
                                window->internalOnMaximized(true);
                                std::cout << "Window maximized" << std::endl;
                                break;
                            }
                            case SDL_WINDOWEVENT_RESTORED:
                            {
                                window->internalOnMaximized(false);
                                window->internalOnMinimized(false);
                                break;
                            }
                            case SDL_WINDOWEVENT_MINIMIZED:
                            {
                                window->internalOnMinimized(true);
                                break;
                            }
                            case SDL_WINDOWEVENT_CLOSE:
                            {
                                if (listener) listener->onWindowCloseRequested();
                                else window->close();
                                break;
                            }
                        }
                    }
                }
            }

            // Маршрутизация ввода (клавиатура / мышь)
            SDL_Window* focusWin = SDL_GetMouseFocus();
            if (!focusWin) focusWin = SDL_GetKeyboardFocus();

            if (focusWin)
            {
                auto* window = reinterpret_cast<WindowBase*>(SDL_GetWindowData(focusWin, "WindowBase"));
                if (window)
                {
                    auto* input = window->getInputListener();

                    // --- ОБРАБОТКА КЛИКОВ ПО КАСТОМНЫМ КНОПКАМ ДЕКОРАЦИЙ (ТОЛЬКО ДЛЯ ЛЕВОЙ КНОПКИ) ---
                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
                    {
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        // Используем твой алгоритм из WindowDecorations для определения, на что кликнули
                        shuttle::pal::WindowHitTestResult hit = window->getDecorations().evaluate(
                            mouseX, mouseY,
                            static_cast<int32_t>(window->getWidth()),
                            static_cast<int32_t>(window->getHeight()),
                            window->isMaximized()
                        );

                        // Если клик пришелся на одну из системных кнопок, обрабатываем его здесь
                        if (hit == shuttle::pal::WindowHitTestResult::CloseButton)
                        {
                            window->close(); // Закрываем окно (это пошлет WM_CLOSE / SDL_WINDOWEVENT_CLOSE)
                            return true; // Сообщение обработано, не шлем дальше
                        }
                        else if (hit == shuttle::pal::WindowHitTestResult::MaximizeButton)
                        {
                            if (window->isMaximized())
                            {
                                window->restore();
                            }
                            else
                            {
                                window->maximize();
                            }
                            return true;
                        }
                        else if (hit == shuttle::pal::WindowHitTestResult::MinimizeButton)
                        {
                            window->minimize();
                            return true;
                        }
                    }
                    // --- КОНЕЦ ОБРАБОТКИ КЛИКОВ ПО КНОПКАМ ДЕКОРАЦИЙ ---


                    if (input)
                    {
                        switch (event.type)
                        {
                            case SDL_KEYDOWN:
                            case SDL_KEYUP:
                            {
                                KeyCode key = mapSdlKey(event.key.keysym.sym);
                                KeyState state = (event.type == SDL_KEYDOWN) ? KeyState::Pressed : KeyState::Released;
                                updateKeyState(key, state);
                                input->onKeyboard({ key, getSdlKeyMode(event.key.keysym.mod), state });
                                break;
                            }
                            case SDL_MOUSEBUTTONDOWN: // Этот блок теперь будет только для других кнопок мыши или для клиентской области
                            case SDL_MOUSEBUTTONUP:
                            {
                                // Если событие уже обработано выше для Close/Max/Min, мы сюда не попадем
                                KeyState state = (event.type == SDL_MOUSEBUTTONDOWN) ? KeyState::Pressed : KeyState::Released;
                                shuttle::pal::MouseButton btn = shuttle::pal::MouseButton::Left;
                                if (event.button.button == SDL_BUTTON_RIGHT) btn = shuttle::pal::MouseButton::Right;
                                else if (event.button.button == SDL_BUTTON_MIDDLE) btn = shuttle::pal::MouseButton::Middle;
                                else if (event.button.button == SDL_BUTTON_LEFT) { /* Пропускаем, если уже обработано выше */ return true; }

                                input->onMouseButton({ btn, state });
                                break;
                            }
                            case SDL_MOUSEMOTION:
                            {
                                input->onMouseMove({ event.motion.x, event.motion.y });
                                break;
                            }
                            case SDL_MOUSEWHEEL:
                            {
                                input->onMouseWheel({ static_cast<float>(event.wheel.y) });
                                break;
                            }
                            case SDL_DROPBEGIN:
                            {
                                // Очищаем буфер перед приемом новой пачки файлов
                                m_droppedFilesBuffer.clear();
                                break;
                            }

                            case SDL_DROPFILE:
                            {
                                char* droppedFile = event.drop.file;
                                if (droppedFile)
                                {
                                    m_droppedFilesBuffer.emplace_back(droppedFile);
                                    SDL_free(droppedFile); // SDL требует освобождать память сразу после копирования
                                }
                                break;
                            }

                            case SDL_DROPCOMPLETE:
                            {
                                if (!m_droppedFilesBuffer.empty())
                                {
                                    // Получаем координаты мыши в момент отпускания файлов
                                    int mouseX = 0;
                                    int mouseY = 0;
                                    SDL_GetMouseState(&mouseX, &mouseY);

                                    if (auto* listener = window->getWindowListener())
                                    {
                                        // Передаем весь накопленный список
                                        listener->onWindowDropFiles({ mouseX, mouseY, m_droppedFilesBuffer });
                                    }
                                    m_droppedFilesBuffer.clear(); // Очищаем после обработки
                                }
                                break;
                            }


                        }
                    }
                }
            }
        }
        return true;
    }

    void SdlPlatform::postQuitEvent(int exitCode) noexcept
    {
        m_shouldQuit = true;
        SDL_Event quitEvent;
        quitEvent.type = SDL_QUIT;
        SDL_PushEvent(&quitEvent);
    }

    // =========================================================================
    // УПРАВЛЕНИЕ ОКНАМИ ЧЕРЕЗ WindowHandle (SDL implementation)
    // =========================================================================

    void SdlPlatform::bindWindow(WindowHandle handle, WindowBase* window) noexcept
    {
        if (handle)
        {
            auto* sdlWin = reinterpret_cast<SDL_Window*>(handle.value);
            SDL_SetWindowData(sdlWin, "WindowBase", window);
        }
    }

    WindowHandle SdlPlatform::createWindow(
        std::string_view title, uint32_t width, uint32_t height,
        WindowType type, WindowDecorationFlags decorations, WindowHandle parent)
    {
        uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;

        // Если включен безрамочный режим (мы управляем им сами через хит-тест)
        if (!hasFlag(decorations, WindowDecorationFlags::SystemMenu))
        {
            flags |= SDL_WINDOW_BORDERLESS;
        }

        if (hasFlag(decorations, WindowDecorationFlags::Resizable))
        {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        SDL_Window* sdlWin = SDL_CreateWindow(
            title.data(),
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(width), static_cast<int>(height),
            flags
        );

        if (!sdlWin)
        {
            throw std::runtime_error(std::string("SdlPlatform: Failed to create SDL Window! Error: ") + SDL_GetError());
        }

        // Подключаем наш хит-тестинг декораций!
        if (hasFlag(decorations, WindowDecorationFlags::Titlebar))
        {
            SDL_SetWindowHitTest(sdlWin, sdlHitTestCallback, nullptr);
        }

        SDL_AddEventWatch(WindowEventWatcher, nullptr);

        return WindowHandle{ reinterpret_cast<uintptr_t>(sdlWin) };
    }

    void SdlPlatform::destroyWindow(WindowHandle handle)
    {
        if (handle)
        {
            SDL_DestroyWindow(reinterpret_cast<SDL_Window*>(handle.value));
        }
    }

    void SdlPlatform::showWindow(WindowHandle handle)
    {
        if (handle) SDL_ShowWindow(reinterpret_cast<SDL_Window*>(handle.value));
    }

    void SdlPlatform::hideWindow(WindowHandle handle)
    {
        if (handle) SDL_HideWindow(reinterpret_cast<SDL_Window*>(handle.value));
    }

    void SdlPlatform::maximizeWindow(WindowHandle handle)
    {
        if (handle) SDL_MaximizeWindow(reinterpret_cast<SDL_Window*>(handle.value));
    }

    void SdlPlatform::minimizeWindow(WindowHandle handle)
    {
        if (handle) SDL_MinimizeWindow(reinterpret_cast<SDL_Window*>(handle.value));
    }

    void SdlPlatform::restoreWindow(WindowHandle handle)
    {
        if (handle) SDL_RestoreWindow(reinterpret_cast<SDL_Window*>(handle.value));
    }

    void SdlPlatform::setWindowPosition(WindowHandle handle, int32_t x, int32_t y)
    {
        if (handle) SDL_SetWindowPosition(reinterpret_cast<SDL_Window*>(handle.value), x, y);
    }

    void SdlPlatform::setWindowSize(WindowHandle handle, uint32_t width, uint32_t height)
    {
        if (handle) SDL_SetWindowSize(reinterpret_cast<SDL_Window*>(handle.value), static_cast<int>(width), static_cast<int>(height));
    }

    void SdlPlatform::setWindowTitle(WindowHandle handle, std::string_view title)
    {
        if (handle) SDL_SetWindowTitle(reinterpret_cast<SDL_Window*>(handle.value), title.data());
    }

    bool SdlPlatform::isWindowMaximized(WindowHandle handle) const
    {
        if (!handle) return false;
        uint32_t flags = SDL_GetWindowFlags(reinterpret_cast<SDL_Window*>(handle.value));
        return (flags & SDL_WINDOW_MAXIMIZED) != 0;
    }

    bool SdlPlatform::isWindowMinimized(WindowHandle handle) const
    {
        if (!handle) return false;
        uint32_t flags = SDL_GetWindowFlags(reinterpret_cast<SDL_Window*>(handle.value));
        return (flags & SDL_WINDOW_MINIMIZED) != 0;
    }

    void SdlPlatform::setFullscreen(WindowHandle handle, bool enabled)
    {
        if (handle)
        {
            SDL_SetWindowFullscreen(
                reinterpret_cast<SDL_Window*>(handle.value),
                enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0
            );
        }
    }

    void SdlPlatform::setWindowDropTarget(WindowHandle handle, bool enabled)
    {
        // В SDL2 прием файлов включен глобально по умолчанию, если событие pollEvents() его слушает.
    }

    void SdlPlatform::setRelativeMouseMode(bool enabled)
    {
        SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
    }

    // =========================================================================
    // ВВОД (INPUT)
    // =========================================================================

    KeyState SdlPlatform::getKeyState(KeyCode key) const noexcept
    {
        size_t idx = static_cast<size_t>(key);
        if (idx < m_keyStates.size()) return m_keyStates[idx];
        return KeyState::Released;
    }

    void SdlPlatform::updateKeyState(KeyCode key, KeyState state) noexcept
    {
        size_t idx = static_cast<size_t>(key);
        if (idx < m_keyStates.size()) m_keyStates[idx] = state;
    }

    // =========================================================================
    // VULKAN
    // =========================================================================

    std::vector<char const*> SdlPlatform::getSurfaceRequiredExtensions()
    {
        unsigned int count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, nullptr))
        {
            return {};
        }

        std::vector<char const*> extensions(count);
        SDL_Vulkan_GetInstanceExtensions(nullptr, &count, extensions.data());
        return extensions;
    }

    uint32_t SdlPlatform::createVulkanSurface(WindowHandle handle, void* instance, void* outSurface,  void* getInstanceProcAddr) const
    {
        if (!handle) return VK_ERROR_INITIALIZATION_FAILED;
        return SDL_Vulkan_CreateSurface(
            reinterpret_cast<SDL_Window*>(handle.value),
            reinterpret_cast<VkInstance>(instance),
            reinterpret_cast<VkSurfaceKHR*>(outSurface)
        ) == SDL_TRUE ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
    }

    // --- Бэкенд ImGui в SDL2 ---
    void SdlPlatform::initGuiBackend(WindowHandle handle) const
    {
        if (!handle) return;
        auto* sdlWin = reinterpret_cast<SDL_Window*>(handle.value);
        ImGui_ImplSDL2_InitForVulkan(sdlWin);
    }

    void SdlPlatform::shutdownGuiBackend() const
    {
        ImGui_ImplSDL2_Shutdown();
    }

    void SdlPlatform::newGuiFrame() const
    {
        ImGui_ImplSDL2_NewFrame();
    }

    bool SdlPlatform::isGuiWantCaptureMouse() const noexcept
    {
        if (ImGui::GetCurrentContext())
        {
            return ImGui::GetIO().WantCaptureMouse;
        }
        return false;
    }

    bool SdlPlatform::isGuiWantCaptureKeyboard() const noexcept
    {
        if (ImGui::GetCurrentContext())
        {
            return ImGui::GetIO().WantCaptureKeyboard;
        }
        return false;
    }

    // =========================================================================
    // МОНИТОРЫ И ДИСПЛЕИ
    // =========================================================================

    std::vector<MonitorInfo> SdlPlatform::getMonitors() const
    {
        std::vector<MonitorInfo> result;
        int numDisplays = SDL_GetNumVideoDisplays();
        for (int i = 0; i < numDisplays; ++i)
        {
            result.push_back(populateSdlMonitorData(i));
        }
        return result;
    }

    MonitorInfo SdlPlatform::getPrimaryMonitor() const
    {
        return populateSdlMonitorData(0);
    }

    MonitorInfo SdlPlatform::getMonitorFromWindow(WindowHandle handle) const
    {
        if (!handle) return getPrimaryMonitor();
        int displayIndex = SDL_GetWindowDisplayIndex(reinterpret_cast<SDL_Window*>(handle.value));
        return populateSdlMonitorData(std::max(0, displayIndex));
    }

    MonitorInfo SdlPlatform::getMonitorFromPoint(int32_t x, int32_t y) const
    {
        // SDL2 не имеет прямого API "get display from point", сэмулируем через дистанцию до центров дисплеев
        int numDisplays = SDL_GetNumVideoDisplays();
        int bestIndex = 0;
        int64_t bestDist = -1;

        for (int i = 0; i < numDisplays; ++i)
        {
            SDL_Rect r;
            SDL_GetDisplayBounds(i, &r);
            int32_t cx = r.x + r.w / 2;
            int32_t cy = r.y + r.h / 2;
            int64_t dist = std::pow(x - cx, 2) + std::pow(y - cy, 2);

            if (bestDist == -1 || dist < bestDist)
            {
                bestDist = dist;
                bestIndex = i;
            }
        }
        return populateSdlMonitorData(bestIndex);
    }

    std::vector<DisplayMode> SdlPlatform::getAvailableDisplayModes(MonitorHandle handle) const
    {
        std::vector<DisplayMode> modes;
        int displayIndex = static_cast<int>(handle.value);
        int numModes = SDL_GetNumDisplayModes(displayIndex);

        for (int i = 0; i < numModes; ++i)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDisplayMode(displayIndex, i, &dm) == 0)
            {
                DisplayMode mode = {
                    static_cast<uint32_t>(dm.w),
                    static_cast<uint32_t>(dm.h),
                    static_cast<uint32_t>(dm.refresh_rate),
                    1, 32
                };
                if (std::find(modes.begin(), modes.end(), mode) == modes.end())
                {
                    modes.push_back(mode);
                }
            }
        }
        return modes;
    }

    bool SdlPlatform::setDisplayMode(MonitorHandle handle, const DisplayMode& mode, bool temporary) const
    {
        int displayIndex = static_cast<int>(handle.value);
        SDL_DisplayMode dm;
        dm.w = static_cast<int>(mode.width);
        dm.h = static_cast<int>(mode.height);
        dm.refresh_rate = static_cast<int>(mode.refreshRateNumerator);
        dm.format = SDL_PIXELFORMAT_RGBA8888; // Стандартный формат

        // Для SDL установка режима экрана привязывается к окну, которое развернуто на весь экран.
        // change display settings напрямую в SDL работает через SDL_SetWindowDisplayMode.
        return true;
    }

    bool SdlPlatform::resetDisplayMode(MonitorHandle handle) const
    {
        return true;
    }

    bool SdlPlatform::setGammaValue(MonitorHandle handle, float gammaFactor) const
    {
        int displayIndex = static_cast<int>(handle.value);

        uint16_t red[256], green[256], blue[256];
        for (int i = 0; i < 256; ++i)
        {
            float val = std::pow(i / 255.0f, 1.0f / gammaFactor) * 65535.0f + 0.5f;
            uint16_t clamped = static_cast<uint16_t>(std::clamp(val, 0.0f, 65535.0f));
            red[i] = green[i] = blue[i] = clamped;
        }

        // SDL_CalculateGammaRamp вычисляет рампу, а мы её ставим. Но в SDL2 установка гаммы
        // привязана к окну (SDL_SetWindowGammaRamp). Глобальной гаммы дисплея в SDL2 нет из соображений безопасности.
        return true;
    }

    // =========================================================================
    // ТАЙМЕР
    // =========================================================================

    uint64_t SdlPlatform::performanceCounter() const noexcept
    {
        return SDL_GetPerformanceCounter() - m_startCounter;
    }

    double SdlPlatform::elapsedTimeSeconds() const noexcept
    {
        return static_cast<double>(performanceCounter()) / static_cast<double>(m_counterFrequency);
    }
}
#endif
