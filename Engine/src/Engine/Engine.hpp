//
// Created by Shagu on 05.09.2026.
//

#ifndef SHUTTLEENGINE_ENGINE_HPP
#define SHUTTLEENGINE_ENGINE_HPP

#include "Handle.hpp"
#include "Resource.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "FenceWaiter/FenceWaiter.hpp"
#include "MainWindow/MainWindow.hpp"
#include "PAL/Common/Window/MainWindow.hpp"
#include "Render/CameraSystem.hpp"
#include "Render/CommonResources.hpp"
#include "Render/FallbackTextures.hpp"
#include "Render/InstanceRemapPass.hpp"
#include "Render/MeshInstancesCountPass.hpp"
#include "Render/PrefixSumPass.hpp"
#include "Render/WorldTransformUpdatePass.hpp"
#include "SwapchainFactory/SwapchainFactory.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::engine {

    struct SceneHandleTag {};
    struct EnvironmentHandleTag {};
    struct FrameResourcesHandleTag {};
    struct FrameGraphHandleTag {};
    struct ViewportResourcesHandleTag {};
    struct SwapchainResourcesHandleTag {};
    struct SurfaceHandleTag {};
    struct ImUiContextHandleTag {};

    /*
     * Строго типизированные хэндлы для ресурсов Shuttle Engine.
     */
    using SceneHandle              = TypedHandle<SceneHandleTag>;
    using EnvironmentHandle        = TypedHandle<EnvironmentHandleTag>;
    using FrameResourcesHandle     = TypedHandle<FrameResourcesHandleTag>;
    using FrameGraphHandle         = TypedHandle<FrameGraphHandleTag>;
    using ViewportResourcesHandle  = TypedHandle<ViewportResourcesHandleTag>;
    using SwapchainResourcesHandle = TypedHandle<SwapchainResourcesHandleTag>;
    using SurfaceHandle            = TypedHandle<SurfaceHandleTag>;
    using ImUiContextHandle        = TypedHandle<ImUiContextHandleTag>;

    struct FrameGraph {
        render::WorldTransformUpdatePass m_worldTransformUpdatePass;
        render::MeshInstancesCountPass m_meshInstanceCountPass;
        render::PrefixSumPass m_prefixSumPass;
        render::InstanceRemapPass m_instanceRemapPass;
        render::MainRenderPass m_mainRenderPass;

        vk::UniquePipeline worldTransformUpdatePassPipeline;
        vk::UniquePipeline meshInstanceCountPassPipeline;
        vk::UniquePipeline prefixSumPassPipeline;
        vk::UniquePipeline instanceRemapPassPipeline;
        vk::UniquePipeline mainPassPipeline;
        vk::UniquePipeline skyboxPipeline;
        vk::UniquePipeline mainPassDebugPipeline;
        vk::UniquePipeline skyboxDebugPipeline;
    };

    struct ViewportResources {
        // Viewport Attachments
        std::vector<AttachmentOutput> m_depthAttachmentOutputs;
        std::vector<AttachmentOutput> m_colorAttachmentOutputs;
        std::vector<AttachmentOutput> m_debugOutputs1;
        std::vector<AttachmentOutput> m_debugOutputs2;
        std::vector<AttachmentOutput> m_debugOutputs3;
        std::vector<AttachmentOutput> m_debugOutputs4;

        std::vector<ImGuiUniqueDescriptorSet> m_colorAttachmentSets;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets1;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets2;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets3;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets4;
    };

    struct ViewportIndices {
        ImU64 releaseTextureIndex;
        ImU64 debug1TextureIndex;
        ImU64 debug2TextureIndex;
        ImU64 debug3TextureIndex;
        ImU64 debug4TextureIndex;
    };

    struct FrameResources {
        // Per-frame Data
        std::vector<vk::UniqueSemaphore> uniqueImageAvailableSemaphores;
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

    struct ImUiContext {
        vk::UniqueDescriptorPool m_descriptorPool;
        void* m_imguiContext = nullptr;
        pal::Platform* m_platform = nullptr;
    };

    struct SwapchainResources
    {
        std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
        Swapchain swapchain;
    };

    class Engine;

    struct SceneDeleter {
        Engine* engine;
        void operator()(SceneHandle sceneHandle) const;
    };

    struct EnvironmentDeleter {
        Engine* engine;
        void operator()(EnvironmentHandle environmentHandle) const;
    };

    struct FrameGraphDeleter {
        Engine* engine;
        void operator()(FrameGraphHandle frameGraphHandle) const;
    };

    struct FrameResourcesDeleter {
        Engine* engine;
        void operator()(FrameResourcesHandle frameResourcesHandle) const;
    };

    struct ViewportResourcesDeleter {
        Engine* engine;
        void operator()(ViewportResourcesHandle viewportResourcesHandle) const;
    };

    struct SwapchainResourcesDeleter {
        Engine* engine;
        void operator()(SwapchainResourcesHandle swapchainResourcesHandle) const;
    };

    struct SurfaceDeleter {
        Engine* engine;
        void operator()(SurfaceHandle surfaceHandle) const;
    };

    struct ImUiContextDeleter {
        Engine* engine;
        void operator()(ImUiContextHandle imUiContextHandle) const;
    };

    template<> struct ResourceTraits<render::UploadSceneOutput> { using handleType = SceneHandle; };
    template<> struct ResourceTraits<render::DeviceEnvironmentResources> { using handleType = EnvironmentHandle; };
    template<> struct ResourceTraits<FrameGraph> { using handleType = FrameGraphHandle; };
    template<> struct ResourceTraits<FrameResources> { using handleType = FrameResourcesHandle; };
    template<> struct ResourceTraits<ViewportResources> { using handleType = ViewportResourcesHandle; };
    template<> struct ResourceTraits<SwapchainResources> { using handleType = SwapchainResourcesHandle; };
    template<> struct ResourceTraits<vk::UniqueSurfaceKHR> { using handleType = SurfaceHandle; };
    template<> struct ResourceTraits<ImUiContext> { using handleType = ImUiContextHandle; };

    using UniqueSceneHandle = UniqueHandle<SceneHandle, SceneDeleter>;
    using UniqueEnvironmentHandle = UniqueHandle<EnvironmentHandle, EnvironmentDeleter>;
    using UniqueFrameGraphHandle = UniqueHandle<FrameGraphHandle, FrameGraphDeleter>;
    using UniqueFrameResourcesHandle = UniqueHandle<FrameResourcesHandle, FrameResourcesDeleter>;
    using UniqueViewportResourcesHandle = UniqueHandle<ViewportResourcesHandle, ViewportResourcesDeleter>;
    using UniqueSwapchainResourcesHandle = UniqueHandle<SwapchainResourcesHandle, SwapchainResourcesDeleter>;
    using UniqueSurfaceHandle = UniqueHandle<SurfaceHandle, SurfaceDeleter>;
    using UniqueImUiContextHandle = UniqueHandle<ImUiContextHandle, ImUiContextDeleter>;

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

        vk::ResultValue<RenderIndices> prepareFrame(FrameResourcesHandle frameResources, SwapchainResourcesHandle swapchain);

        vk::ResultValue<EnvironmentHandle> createEnvironmentResources(
            std::filesystem::path const &filePath);

        vk::ResultValue<SceneHandle> uploadScene(
            render::LoadedSceneData const &sceneData
        );

        vk::ResultValue<UniqueSceneHandle> uploadSceneUnique(
            render::LoadedSceneData const &sceneData
        );

        vk::ResultValue<UniqueEnvironmentHandle> createEnvironmentUnique(
            std::filesystem::path const &filePath
        );
        /**
         * Creates a frame graph that encapsulates the rendering passes and their dependencies.
         * @return A ResultValue containing the FrameGraphHandle if successful, or an error code if failed.
         */
        vk::ResultValue<FrameGraphHandle> createFrameGraph();
        vk::ResultValue<UniqueFrameGraphHandle> createFrameGraphUnique();

        vk::ResultValue<FrameResourcesHandle> createFrameResources(
            SceneHandle sceneHandle,
            FrameResourcesHandle oldResources = NullHandle);
        vk::ResultValue<UniqueFrameResourcesHandle> createFrameResourcesUnique(
            SceneHandle scene,
            FrameResourcesHandle oldResources = NullHandle);

        vk::ResultValue<ViewportResourcesHandle> createViewportResources(vk::Extent2D viewportExtent);
        vk::ResultValue<UniqueViewportResourcesHandle> createViewportResourcesUnique(vk::Extent2D viewportExtent);
        vk::ResultValue<ViewportIndices> getViewportIndices(ViewportResourcesHandle viewportResourcesHandle, uint32_t frameIndex);

        vk::ResultValue<SurfaceHandle> createSurface(pal::Platform const &platform, pal::WindowHandle const &window);
        vk::ResultValue<UniqueSurfaceHandle> createSurfaceUnique(pal::Platform& platform, pal::WindowHandle& window);

        vk::ResultValue<SwapchainResourcesHandle> createSwapchainResources(
            SurfaceHandle surfaceHandle,
            vk::Extent2D viewportExtent,
            SwapchainResourcesHandle oldSwapchainResourcesHandle = NullHandle);

        vk::ResultValue<UniqueSwapchainResourcesHandle> createSwapchainResourcesUnique(
            SurfaceHandle surfaceHandle,
            vk::Extent2D viewportExtent,
            SwapchainResourcesHandle oldSwapchainResourcesHandle = NullHandle);

        vk::ResultValue<ImUiContextHandle> createImUiContext(

        );

        void renderUiOnly();
        void renderSceneUi();

        void destroyScene(SceneHandle sceneHandle);
        void destroyEnvironment(EnvironmentHandle environmentHandle);
        void destroyFrameGraph(FrameGraphHandle frameGraphHandle);
        void destroyFrameResources(FrameResourcesHandle frameResourcesHandle);
        void destroyViewportResources(ViewportResourcesHandle viewportResourcesHandle);
        void destroySurface(SurfaceHandle surfaceHandle);
        void destroySwapchainResources(SwapchainResourcesHandle swapchainResourcesHandle);

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
        vk::detail::DynamicLoader m_dynamicLoader{};
        vk::detail::DispatchLoaderDynamic m_dispatcher{};

        vk::UniqueInstance m_uniqueInstance;
        vk::UniqueDebugUtilsMessengerEXT m_messenger;
        vk::UniqueSurfaceKHR m_uniqueSurface;

        vk::PhysicalDevice m_physicalDevice;
        vk::UniqueDevice m_uniqueDevice;
        vk::Device m_device;
        vk::Queue m_graphicsQueue;
        uint32_t m_graphicsQueueFamilyIndex = 0;

        render::DescriptorHeapSet m_descriptorHeapSet;
        resources::UniqueAllocator m_allocator;

        FenceWaiter m_fenceWaiter;

        static constexpr uint32_t m_frameCount = 2U;
        SwapchainResources m_activeResources;
        std::vector<vk::ImageLayout> m_swapchainImageLayouts;

        render::RenderContext m_renderContext;
        vk::UniqueCommandPool m_uniqueGraphicsCommandPool;
        render::CommonResources m_commonResources;
        render::FallbackTextures m_fallbackTextures;

        SlotMapStorage<render::UploadSceneOutput> m_uploadedScenes;
        SlotMapStorage<render::DeviceEnvironmentResources> m_environmentResources;
        SlotMapStorage<FrameGraph> m_frameGraphs;
        SlotMapStorage<FrameResources> m_frameResources;
        SlotMapStorage<ViewportResources> m_viewportResources;
        SlotMapStorage<SwapchainResources> m_swapchainResources;
        SlotMapStorage<vk::UniqueSurfaceKHR> m_surfaces;
        ImGuiContextM m_imGuiContext;

        // Линейная lock-free шкала времени Shuttle Engine
        std::atomic<uint64_t>      m_currentTimelineId{0};
        std::atomic<uint64_t>      m_lastCompletedTimelineId{0};

        vk::UniquePipelineLayout m_pipelineLayout;

        std::vector<vk::UniqueCommandBuffer> m_uniqueGraphicsCommandBuffers;

        [[nodiscard]] vk::ResultValue<vk::ShaderModule> createShaderModule(std::filesystem::path const &shaderPath);
        [[nodiscard]] vk::ResultValue<vk::UniqueShaderModule> createShaderModuleUnique(std::filesystem::path const &shaderPath) const;

        vk::ResultValue<vk::UniquePipeline> createComputePipeline(
            std::filesystem::path const &shaderPath);

        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createMainRenderPipeline(
            vk::Format colorFormat,
            vk::Format depthFormat,
            bool enableDebug) const;

        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createSkyboxPipeline(
            vk::Format colorFormat,
            vk::Format depthFormat,
            bool enableDebug = false) const;

        void writeRenderCommands(
            vk::CommandBuffer cmdBuffer,
            FrameGraphHandle renderGraph,
            FrameResourcesHandle frameResources,
            ViewportResourcesHandle viewportResources,
            SceneHandle scene,
            EnvironmentHandle environment);
    };
}
#endif //SHUTTLEENGINE_ENGINE_HPP
