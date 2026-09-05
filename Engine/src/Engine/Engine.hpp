//
// Created by Shagu on 05.09.2026.
//

#ifndef SHUTTLEENGINE_ENGINE_HPP
#define SHUTTLEENGINE_ENGINE_HPP

#include "IncludeVulkan.hpp"
#include "CameraController/CameraController.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "MainWindow/MainWindow.hpp"
#include "PAL/Common/Window/MainWindow.hpp"
#include "Render/CameraSystem.hpp"
#include "Render/CommonResources.hpp"
#include "Render/FallbackTextures.hpp"
#include "Render/InstanceRemapPass.hpp"
#include "Render/MeshInstancesCountPass.hpp"
#include "Render/PrefixSumPass.hpp"
#include "Render/WorldTransformUpdatePass.hpp"
#include "RetireController/RetireController.hpp"
#include "SwapchainFactory/SwapchainFactory.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine {
    class Engine {
    public:
        Engine(
            pal::Platform&  platform,
            pal::WindowHandle& windowHandle,
            pal::WindowBase& window);
        ~Engine();

        // Запрет копирования
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        struct RenderIndices
        {
            uint32_t frameIndex = 0;
            uint32_t imageIndex = 0;
        };

        void createViewPortResources(vk::Extent2D viewportExtent, core::Camera &camera);
        void recreateViewportResources(vk::Extent2D viewportExtent, core::Camera &camera);
        void recreateAllResources(pal::MainWindow &window);

        void updateFrameData(render::UploadSceneOutput const *scene);
        vk::ResultValue<RenderIndices> prepareFrame();
        [[nodiscard]] bool hasFrameResources() const;
        void setHasFrameResources(bool hasFrameResources);

        vk::ResultValue<render::DeviceEnvironmentResources> createEnvironmentResources(
            std::filesystem::path const &filePath);

        vk::ResultValue<render::UploadSceneOutput> uploadScene(
            render::LoadedSceneData const &sceneData
        );

        vk::Result waitRenderIdle();

        void doFrameRender(
            editor::core::MainWindow &mainWindow,
            core::Camera &camera,
            RenderIndices renderIndices,
            render::UploadSceneOutput const &scene,
            render::DeviceEnvironmentResources const &environment,
            bool isResizeMode);

        void drawFrame(bool isResizeMode, float dt, pal::MainWindow &main_window, editor::core::MainWindow &mainWindow, core::Camera &camera, render
                       ::UploadSceneOutput const *scene, render::DeviceEnvironmentResources const *environment);

    private:
        vk::UniqueInstance m_uniqueInstance;
        vk::UniqueDebugUtilsMessengerEXT m_messenger;
        vk::UniqueSurfaceKHR m_uniqueSurface;

        vk::PhysicalDevice m_physicalDevice;
        vk::Device m_device;
        vk::UniqueDevice m_uniqueDevice;
        vk::Queue m_graphicsQueue;
        uint32_t m_graphicsQueueFamilyIndex = 0;

        render::DescriptorHeapSet m_descriptorHeapSet;
        resources::UniqueAllocator m_allocator;
        ImGuiContextM m_imGuiContext;

        static constexpr uint32_t m_frameCount = 2U;
        SwapchainContext m_swapchainContext;
        SwapchainResources m_activeResources;
        std::vector<vk::ImageLayout> m_swapchainImageLayouts;
        RetireController m_retireController;

        render::RenderContext m_renderContext;
        vk::UniqueCommandPool m_uniqueGraphicsCommandPool;
        render::CommonResources m_commonResources;
        render::FallbackTextures m_fallbackTextures;

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
        bool m_hasFrameResources = false;

        std::vector<vk::UniqueCommandBuffer> m_uniqueGraphicsCommandBuffers;

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
    };
}


#endif //SHUTTLEENGINE_ENGINE_HPP
