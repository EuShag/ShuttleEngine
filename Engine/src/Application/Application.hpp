/**
 * @file Application.hpp
 * @brief Main application class that orchestrates Vulkan and UI logic.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <optional>

#include <SDL2/SDL.h>
#include <vulkan/vulkan.hpp>

#include "portable-file-dialogs.h"
#include "Sdl.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Camera/Camera.hpp"
#include "CameraController/CameraController.hpp"
#include "Render/Render.hpp"
#include "MainWindow/MainWindow.hpp"
#include "Render/CameraSystem.hpp"
#include "Render/CommonResources.hpp"
#include "Render/FallbackTextures.hpp"
#include "Render/InstanceRemapPass.hpp"
#include "Render/MainPass.hpp"
#include "Render/MeshInstancesCountPass.hpp"
#include "Render/PrefixSumPass.hpp"
#include "Render/WorldTransformUpdatePass.hpp"
#include "RetireController/RetireController.hpp"
#include "SwapchainFactory/SwapchainFactory.hpp"
#include "UiRender/UiRender.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"
#include "Assets/SceneCompiler/SceneCompiler.hpp"
#include "Assets/EnvironmentCompiler/CompiledEnvironment.hpp"

namespace shuttle::engine
{

    /**
     * @class Application
     * @brief Main application class that orchestrates the engine lifecycle, Vulkan resources, and main loop.
     */
    class Application
    {
    public:
        /**
         * @brief Constructs the application and initializes the Vulkan context.
         * @param argc Argument count from main.
         * @param argv Argument vector from main.
         */
        Application(int argc, char** argv);

        /**
         * @brief Safely shuts down the application and waits for GPU idle.
         */
        ~Application();

        /**
         * @brief Starts the main application loop.
         * @return Exit code for main.
         */
        int run();

    private:
        /**
         * @brief Creates attachment resources for the viewport.
         * @param viewportExtent The extent (width, height) of the viewport.
         */
        void createViewPortResources(vk::Extent2D viewportExtent);

        /**
         * @brief Recreates viewport resources (e.g., on window resize).
         * @param viewportExtent The new extent (width, height) of the viewport.
         */
        void recreateViewportResources(vk::Extent2D viewportExtent);

        /**
         * @brief Recreates all swapchain-dependent resources.
         */
        void recreateAllResources();

        /**
         * @brief Synchronizes internal pointers to the active scene and environment data.
         */
        void syncPointers();

        /**
         * @brief Loads a pre-compiled scene file (.sblb) from disk and uploads to GPU.
         * @param path The filesystem path to the scene file.
         */
        void loadScene(std::filesystem::path const& path);

        /**
         * @brief Loads a pre-compiled environment file (.env) from disk and uploads to GPU.
         * @param path The filesystem path to the environment file.
         */
        void loadEnvironment(std::filesystem::path const& path);

        /**
         * @brief Prepares per-frame GPU buffers and systems based on the currently active scene.
         */
        void updateFrameData();

        /**
         * @brief Executes the rendering logic for a single frame.
         * @param isResizeMode True if the frame is being drawn during a window resize operation.
         * @param dt Delta time in seconds since the last frame.
         */
        void drawFrame(bool isResizeMode, float dt);

        /**
         * @struct OpenAsset
         * @brief Represents an asset (scene or environment) loaded into the engine and UI.
         */
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

        // Core Systems
        editor::core::ResourceId m_nextId = 1;
        SdlLibrary m_sdlLibrary;
        SdlWindow m_window;

        vk::UniqueInstance m_uniqueInstance;
        vk::UniqueDebugUtilsMessengerEXT m_messenger;
        vk::UniqueSurfaceKHR m_uniqueSurface;

        vk::PhysicalDevice m_physicalDevice;
        vk::Device m_device;
        vk::UniqueDevice m_uniqueDevice;
        vk::Queue m_graphicsQueue;
        uint32_t m_graphicsQueueFamilyIndex = 0;

        resources::UniqueAllocator m_allocator;
        static constexpr uint32_t m_frameCount = 2U;
        SwapchainContext m_swapchainContext;
        SwapchainResources m_activeResources;
        std::vector<vk::ImageLayout> m_swapchainImageLayouts;
        RetireController m_retireController;

        render::RenderContext m_renderContext;
        vk::UniqueCommandPool m_uniqueGraphicsCommandPool;
        render::DescriptorHeapSet m_descriptorHeapSet;
        render::CommonResources m_commonResources;
        render::FallbackTextures m_fallbackTextures;

        core::Camera m_camera;
        core::CameraController m_cameraController;

        uint32_t m_currentFrameIndex = 0U;
        UiRender m_uiRender;
        std::vector<vk::UniqueCommandBuffer> m_uniqueGraphicsCommandBuffers;

        bool m_isMinimized = false;
        editor::core::MainWindow m_mainWindow;

        // Viewport Attachments
        std::vector<AttachmentOutput> m_depthAttachmentOutputs;
        std::vector<AttachmentOutput> m_colorAttachmentOutputs;
        std::vector<AttachmentOutput> m_debugOutputs1;
        std::vector<AttachmentOutput> m_debugOutputs2;
        std::vector<AttachmentOutput> m_debugOutputs3;
        std::vector<AttachmentOutput> m_debugOutputs4;

        std::vector<editor::core::UniqueViewportDescriptorSet> m_colorAttachmentSets;
        std::vector<editor::core::UniqueViewportDescriptorSet> m_debugAttachmentSets1;
        std::vector<editor::core::UniqueViewportDescriptorSet> m_debugAttachmentSets2;
        std::vector<editor::core::UniqueViewportDescriptorSet> m_debugAttachmentSets3;
        std::vector<editor::core::UniqueViewportDescriptorSet> m_debugAttachmentSets4;

        editor::core::ResourceBin m_resourceBin;
        vk::UniquePipelineLayout m_pipelineLayout;

        render::WorldTransformUpdatePass m_worldTransformUpdatePass;
        render::MeshInstancesCountPass m_meshInstanceCountPass;
        render::PrefixSumPass m_prefixSumPass;
        render::InstanceRemapPass m_instanceRemapPass;
        render::MainRenderPass m_mainRenderPass;

        // State
        bool m_hasScene = false;
        bool m_hasEnvironment = false;
        bool m_hasFrameResources = false;

        std::vector<OpenAsset> m_openAssets;
        editor::core::ResourceId m_activeSceneId = 0;
        editor::core::ResourceId m_activeEnvironmentId = 0;

        render::UploadSceneOutput* m_scene = nullptr;
        render::DeviceEnvironmentResources* m_deviceEnvironmentResources = nullptr;

        // Per-frame Data
        std::vector<render::CameraSystem> m_cameraSystems;
        std::vector<render::MainPassSettingSystem> m_mainPassSettingSystems;
        std::vector<resources::UniqueAllocatedBuffer> m_worldTransformBuffers;
        std::vector<resources::UniqueAllocatedBuffer> m_instanceRemapBuffers;
        std::vector<resources::UniqueAllocatedBuffer> m_meshInstanceCursorBuffers;
        std::vector<resources::UniqueAllocatedBuffer> m_indirectDrawCommandsBuffers;

        std::vector<vk::DeviceAddress> m_worldTransformBufferAddresses;
        std::vector<vk::DeviceAddress> m_instanceRemapBufferAddresses;
        std::vector<vk::DeviceAddress> m_meshInstanceCursorBufferAddresses;
        std::vector<vk::DeviceAddress> m_indirectDrawCommandsBufferAddresses;

        std::chrono::high_resolution_clock::time_point m_lastTime;
        float m_deltaTime = 0.0f;
        float m_totalTime = 0.0f;
    };

} // namespace shuttle::engine
