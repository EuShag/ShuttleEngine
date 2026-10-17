/**
 * @file application.hpp
 * @brief Main application class that orchestrates Vulkan and UI logic.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * license file in the root directory of this source tree.
 */

#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

#include "portable-file-dialogs.h"

// Кроссплатформенный слой PAL
#include "pal/platform.hpp"
#include "pal/common/events/events.hpp"
#include "pal/common/window/main_window.hpp"

#include "camera/camera.hpp"
#include "camera_controller/camera_controller.hpp"
#include "render/render.hpp"
#include "main_window/main_window.hpp"
#include "assets/scene_compiler/scene_compiler.hpp"
#include "assets/environment_compiler/compiled_environment.hpp"
#include "engine/engine_impl.hpp"

namespace shuttle::engine
{
    /**
     * @class Application
     * @brief Main application class that orchestrates the engine lifecycle, Vulkan resources, and main loop.
     */
    class Application : public pal::IWindowListener, public input::IInputListener
    {
    public:
        Application(int argc, char** argv);
        ~Application() override = default;

        int run();

        // --- Обработчики событий PAL ---
        void onWindowResize(const pal::WindowResizeEvent& event) override;
        void onWindowCloseRequested() override;
        void onWindowPaint() override;
        void onKeyboard(const input::KeyboardEvent& event) override;
        void onMouseButton(const input::MouseButtonEvent& e) override {};
        void onMouseMove(const input::MouseMoveEvent& e) override {};
        void onMouseWheel(const input::MouseWheelEvent& e) override {};

    private:
        const render::UploadSceneOutput* findActiveScene() const;
        const render::DeviceEnvironmentResources* findActiveEnvironment() const;

        void loadScene(std::filesystem::path const& path);
        void loadEnvironment(std::filesystem::path const& path);

        struct RenderIndices
        {
            uint32_t frameIndex = 0;
            uint32_t imageIndex = 0;
        };

        struct OpenAsset
        {
            editor::core::ResourceId id = 0;
            std::string name;
            std::filesystem::path path;
            bool isScene = true;
            bool isDirty = false;

            std::optional<render::UploadSceneOutput> sceneGpuData;
            std::optional<render::DeviceEnvironmentResources> envGpuData;
            std::optional<assets::scene_compiler::CompiledScene> compiledSceneRAM;
            std::optional<assets::environment_compiler::CompiledEnvironment> compiledEnvRAM;
        };

        // Core PAL Systems (Строгий порядок объявления!)
        editor::core::ResourceId m_nextId = 1;
        pal::Platform            m_platform;
        pal::WindowHandle        m_windowHandle;
        pal::MainWindow          m_window;

        core::Camera m_camera;
        core::CameraController m_cameraController;

        bool m_isMinimized = false;
        editor::core::MainWindow m_mainWindow;

        std::vector<OpenAsset> m_openAssets;
        editor::core::ResourceId m_activeSceneId = 0;
        editor::core::ResourceId m_activeEnvironmentId = 0;

        EngineImpl m_engine;

        std::chrono::high_resolution_clock::time_point m_lastTime;
        float m_deltaTime = 0.0f;
        float m_totalTime = 0.0f;
    };

} // namespace shuttle::engine