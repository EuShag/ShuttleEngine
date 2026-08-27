#pragma once

#include <string_view>
#include <vector>
#include <array>

#include "PAL/Common/Display/DisplayTypes.hpp"
#include "PAL/Common/Display/MonitorInfo.hpp"
#include "PAL/Common/Display/MonitorHandle.hpp"
#include "PAL/Common/Keyboard/KeyCode.hpp"
#include "PAL/Common/Keyboard/KeyState.hpp"
#include "PAL/Common/Window/Decorations/WindowDecorationFlags.hpp"
#include "PAL/Common/Window/WindowType.hpp"
#include "PAL/Common/Window/WindowBase.hpp"

namespace shuttle::pal {
    class WindowBase;
}

namespace shuttle::pal::impl
{
    class SdlPlatform
    {
    public:
        SdlPlatform();
        ~SdlPlatform();

        // Запрет копирования/перемещения платформы (она должна быть одна на бэкенд)
        SdlPlatform(const SdlPlatform&) = delete;
        SdlPlatform& operator=(const SdlPlatform&) = delete;
        SdlPlatform(SdlPlatform&&) = delete;
        SdlPlatform& operator=(SdlPlatform&&) = delete;

        [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

        // ---------------------------------------------------------------------
        // Главный цикл и события
        // ---------------------------------------------------------------------
        [[nodiscard]] bool pollEvents();
        void postQuitEvent(int exitCode = 0) noexcept;
        [[nodiscard]] bool shouldQuit() const noexcept { return m_shouldQuit; }

        // ---------------------------------------------------------------------
        // Управление окнами (Аналог Win32Platform API)
        // ---------------------------------------------------------------------
        [[nodiscard]] WindowHandle createWindow(
            std::string_view title, uint32_t width, uint32_t height,
            WindowType type, WindowDecorationFlags decorations,
            WindowHandle parent = {});

        void destroyWindow(WindowHandle handle);

        // Связывает нативный SDL_Window* с объектом WindowBase
        void bindWindow(WindowHandle handle, WindowBase* window) noexcept;

        void showWindow(WindowHandle handle);
        void hideWindow(WindowHandle handle);
        void maximizeWindow(WindowHandle handle);
        void minimizeWindow(WindowHandle handle);
        void restoreWindow(WindowHandle handle);

        void setWindowPosition(WindowHandle handle, int32_t x, int32_t y);
        void setWindowSize(WindowHandle handle, uint32_t width, uint32_t height);
        void setWindowTitle(WindowHandle handle, std::string_view title);

        [[nodiscard]] bool isWindowMaximized(WindowHandle handle) const;
        [[nodiscard]] bool isWindowMinimized(WindowHandle handle) const;

        void setFullscreen(WindowHandle handle, bool enabled);
        void setWindowDropTarget(WindowHandle handle, bool enabled);
        static void setRelativeMouseMode(bool enabled);

        // ---------------------------------------------------------------------
        // Ввод
        // ---------------------------------------------------------------------
        [[nodiscard]] KeyState getKeyState(KeyCode key) const noexcept;
        void updateKeyState(KeyCode key, KeyState state) noexcept;

        // ---------------------------------------------------------------------
        // Vulkan Интеграция
        // ---------------------------------------------------------------------
        [[nodiscard]] static std::vector<char const*> getSurfaceRequiredExtensions();
        [[nodiscard]] uint32_t createVulkanSurface(WindowHandle handle, void *instance, void *outSurface, void* getInstanceProcAddr = nullptr) const;



        // ---------------------------------------------------------------------
        // Платформенный бэкенд ImGui
        // ---------------------------------------------------------------------
        void initGuiBackend(WindowHandle handle) const;
        void shutdownGuiBackend() const;
        void newGuiFrame() const;

        [[nodiscard]] bool isGuiWantCaptureMouse() const noexcept;
        [[nodiscard]] bool isGuiWantCaptureKeyboard() const noexcept;

        // ---------------------------------------------------------------------
        // Мониторы
        // ---------------------------------------------------------------------
        [[nodiscard]] std::vector<MonitorInfo> getMonitors() const;
        [[nodiscard]] MonitorInfo getPrimaryMonitor() const;
        [[nodiscard]] MonitorInfo getMonitorFromWindow(WindowHandle handle) const;
        [[nodiscard]] MonitorInfo getMonitorFromPoint(int32_t x, int32_t y) const;

        [[nodiscard]] std::vector<DisplayMode> getAvailableDisplayModes(MonitorHandle handle) const;
        [[nodiscard]] bool setDisplayMode(MonitorHandle handle, const DisplayMode& mode, bool temporary = true) const;
        [[nodiscard]] bool resetDisplayMode(MonitorHandle handle) const;
        [[nodiscard]] bool setGammaValue(MonitorHandle handle, float gammaFactor) const;

        // ---------------------------------------------------------------------
        // Таймер
        // ---------------------------------------------------------------------
        [[nodiscard]] uint64_t performanceCounter() const noexcept;
        [[nodiscard]] double elapsedTimeSeconds() const noexcept;

    private:
        bool m_initialized = false;
        bool m_shouldQuit = false;

        std::array<KeyState, static_cast<size_t>(KeyCode::Count)> m_keyStates{};

        uint64_t m_startCounter = 0;
        uint64_t m_counterFrequency = 0;
        std::vector<std::string> m_droppedFilesBuffer{};
    };
}