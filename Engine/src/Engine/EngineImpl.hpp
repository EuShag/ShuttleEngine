//
// Created by Shagu on 05.09.2026.
//

#ifndef SHUTTLEENGINE_ENGINE_HPP
#define SHUTTLEENGINE_ENGINE_HPP

#include "Handle.hpp"
#include "Resource.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "FenceWaiter/FenceWaiter.hpp"
#include "ImUiContext/ImUiContext.hpp"
#include "MainWindow/MainWindow.hpp"
#include "Render/CameraSystem.hpp"
#include "Render/CommonResources.hpp"
#include "Render/FallbackTextures.hpp"
#include "Engine/ViewportResources.hpp"
#include "IEngineImpl.hpp"

namespace shuttle::engine {

    struct FrameGraph {
        vk::UniquePipeline worldTransformUpdatePassPipeline;
        vk::UniquePipeline meshInstanceCountPassPipeline;
        vk::UniquePipeline prefixSumPassPipeline;
        vk::UniquePipeline instanceRemapPassPipeline;
        vk::UniquePipeline mainPassPipeline;
        vk::UniquePipeline skyboxPipeline;
        vk::UniquePipeline mainPassDebugPipeline;
        vk::UniquePipeline skyboxDebugPipeline;
    };

    struct FrameResources {
        // Per-frame Data
        std::vector<vk::Result> frameStatuses;
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

    struct SwapchainResources
    {
        std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
        vk::UniqueSwapchainKHR swapchain;
        vk::Extent2D extent;
        vk::Format format;
        std::vector<vk::Image> images;
        std::vector<vk::UniqueImageView> imageViews;
        uint32_t imageCount{0};
    };

    class EngineImpl;

    struct SceneDeleter {
        EngineImpl* engine{};
        void operator()(SceneHandle sceneHandle) const;
    };

    struct EnvironmentDeleter {
        EngineImpl* engine{};
        void operator()(EnvironmentHandle environmentHandle) const;
    };

    struct FrameResourcesDeleter {
        EngineImpl* engine{};
        void operator()(FrameResourcesHandle frameResourcesHandle) const;
    };

    struct ViewportResourcesDeleter {
        EngineImpl* engine{};
        void operator()(ViewportResourcesHandle viewportResourcesHandle) const;
    };

    struct SwapchainResourcesDeleter {
        EngineImpl* engine{};
        void operator()(SwapchainResourcesHandle swapchainResourcesHandle) const;
    };

    struct SurfaceDeleter {
        EngineImpl* engine{};
        void operator()(SurfaceHandle surfaceHandle) const;
    };

    struct ImUiContextDeleter {
        EngineImpl* engine{};
        void operator()(ImUiContextHandle imUiContextHandle) const;
    };

    template<> struct ResourceTraits<render::UploadSceneOutput> { using handleType = SceneHandle; };
    template<> struct ResourceTraits<render::DeviceEnvironmentResources> { using handleType = EnvironmentHandle; };
    template<> struct ResourceTraits<FrameResources> { using handleType = FrameResourcesHandle; };
    template<> struct ResourceTraits<ViewportResources> { using handleType = ViewportResourcesHandle; };
    template<> struct ResourceTraits<SwapchainResources> { using handleType = SwapchainResourcesHandle; };
    template<> struct ResourceTraits<vk::UniqueSurfaceKHR> { using handleType = SurfaceHandle; };
    template<> struct ResourceTraits<ImUiContext> { using handleType = ImUiContextHandle; };

    using UniqueSceneHandle = UniqueHandle<SceneHandle, SceneDeleter>;
    using UniqueEnvironmentHandle = UniqueHandle<EnvironmentHandle, EnvironmentDeleter>;
    using UniqueFrameResourcesHandle = UniqueHandle<FrameResourcesHandle, FrameResourcesDeleter>;
    using UniqueViewportResourcesHandle = UniqueHandle<ViewportResourcesHandle, ViewportResourcesDeleter>;
    using UniqueSwapchainResourcesHandle = UniqueHandle<SwapchainResourcesHandle, SwapchainResourcesDeleter>;
    using UniqueSurfaceHandle = UniqueHandle<SurfaceHandle, SurfaceDeleter>;
    using UniqueImUiContextHandle = UniqueHandle<ImUiContextHandle, ImUiContextDeleter>;

    struct CommandPool{
        vk::UniqueCommandPool commandPool;
        moodycamel::ConcurrentQueue<vk::CommandBuffer> primaryCommandBufferPool;
        moodycamel::ConcurrentQueue<vk::CommandBuffer> secondaryCommandBufferPool;
    };

    class EngineImpl final: public IEngineImpl {
        struct CopyBufferCommandInfo
        {
            vk::DeviceSize stagingOffset;
            vk::Buffer destinationBuffer;
            vk::DeviceSize dataSize;
        };

        struct ImageBarrierInfo
        {
            vk::Image image{};
            uint32_t mipLevels{};
        };

        struct BufferBarrierInfo
        {
            vk::Buffer buffer{};
            vk::DeviceSize size{};
        };

        struct ImageMipUploadInfo
        {
            uint32_t imageIndex{};

            uint32_t mipLevel{};

            vk::DeviceSize sourceOffset{};
            vk::DeviceSize stagingOffset{};

            uint32_t width{};
            uint32_t height{};

            vk::DeviceSize dataSize{};
        };

        struct EnvironmentUploadOffsets {
            vk::DeviceSize environmentInfo;

            std::vector<ImageMipUploadInfo> skyboxMips;
            std::vector<ImageMipUploadInfo> irradianceMips;
            std::vector<ImageMipUploadInfo> prefilterMips;

            vk::DeviceSize totalSize;
        };

        struct SceneUploadOffsets
        {
            vk::DeviceSize sceneInfo{};
            vk::DeviceSize nodes{};
            vk::DeviceSize levels{};
            vk::DeviceSize transforms{};
            vk::DeviceSize drawables{};

            vk::DeviceSize meshes{};
            vk::DeviceSize positions{};
            vk::DeviceSize attributes{};
            vk::DeviceSize indices{};
            vk::DeviceSize materials{};

            vk::DeviceSize directionalLights{};

            std::vector<ImageMipUploadInfo> textureMipUploadInfos;

            vk::DeviceSize totalSize{};
        };

    public:
        EngineImpl() = default;
        ~EngineImpl() override;

        vk::Result initFrameGraph(vk::Format colorAttachmentFormat, vk::Format depthAttachmentFormat);

        [[nodiscard]] static vk::ResultValue<IEngineImpl*> create();

        // Запрет копирования
        EngineImpl(const EngineImpl&) = delete;
        EngineImpl& operator=(const EngineImpl&) = delete;
        EngineImpl(EngineImpl&&)  noexcept {}
        EngineImpl& operator=(EngineImpl&&)  noexcept { return *this; };

        struct RenderIndices
        {
            uint32_t frameIndex = 0;
            uint32_t imageIndex = 0;
        };

        [[nodiscard]] vk::ResultValue<EnvironmentHandle> createEnvironmentResources(
            render::LoadedEnvironmentData const &data) override;

        [[nodiscard]] vk::ResultValue<SceneHandle> uploadScene(
            render::LoadedSceneData const &sceneData
        ) override;

        [[nodiscard]] vk::ResultValue<FrameResourcesHandle> createFrameResources(
            SceneHandle sceneHandle,
            FrameResourcesHandle oldResources) override;

        [[nodiscard]] vk::ResultValue<ViewportResourcesHandle> createViewportResources(vk::Extent2D viewportExtent) override;

        [[nodiscard]] vk::ResultValue<ViewportIndices> getViewportIndices(
            ImUiContextHandle imUiContextHandle,
            ViewportTexturesHandle viewportTexturesHandle,
            uint32_t index) override;

        [[nodiscard]] vk::ResultValue<ViewportTexturesHandle> createViewportTextures(
            ImUiContextHandle imUiContextHandle,
            ViewportResourcesHandle viewportResourcesHandle
        ) override;

        [[nodiscard]] vk::ResultValue<SurfaceHandle> createSurface(pal::Platform const &platform, pal::WindowHandle const &window) override;

        [[nodiscard]] vk::ResultValue<SwapchainResourcesHandle> createSwapchainResources(
            SurfaceHandle surfaceHandle,
            vk::Extent2D extent,
            SwapchainResourcesHandle oldSwapchainResourcesHandle) override;

        [[nodiscard]] vk::ResultValue<ImUiContextHandle> createImUiContext(
            pal::WindowBase& window,
            SwapchainResourcesHandle swapchainResources
        ) override;

        [[nodiscard]] vk::Result renderUiOnly(
            IUiPainter &uiPainter,
            ImUiContextHandle imUiContextHandle,
            FrameResourcesHandle frameResourcesHandle,
            ViewportResourcesHandle viewportResourcesHandle,
            ViewportTexturesHandle viewportTexturesHandle,
            SwapchainResourcesHandle swapchainResourcesHandle
        ) override;

        [[nodiscard]] vk::Result renderSceneUi(
            bool isDebugMode,
            IUiPainter &uiPainter,
            render::MainPassSettings const &renderPassSettings,
            core::Camera const &camera,
            ImUiContextHandle imUiContextHandle,
            ViewportResourcesHandle viewportResourcesHandle,
            ViewportTexturesHandle viewportTexturesHandle,
            SwapchainResourcesHandle swapchainResourcesHandle,
            FrameResourcesHandle frameResourcesHandle,
            SceneHandle sceneHandle,
            EnvironmentHandle environmentHandle
        ) override;

        void destroyScene(SceneHandle sceneHandle) override;
        void destroyEnvironment(EnvironmentHandle environmentHandle) override;
        void destroyFrameResources(FrameResourcesHandle frameResourcesHandle) override;
        void destroyViewportResources(ViewportResourcesHandle viewportResourcesHandle) override;
        void destroySurface(SurfaceHandle surfaceHandle) override;
        void destroySwapchainResources(SwapchainResourcesHandle swapchainResourcesHandle) override;
        void destroyImUiContext(ImUiContextHandle typed_handle) override;

    private:
        vk::detail::DynamicLoader m_dynamicLoader{};
        vk::detail::DispatchLoaderDynamic m_dispatcher{};

        vk::UniqueInstance m_uniqueInstance{};

        vk::PhysicalDevice m_physicalDevice{};
        vk::UniqueDevice m_uniqueDevice{};
        vk::Queue m_graphicsQueue{};
        uint32_t m_graphicsQueueFamilyIndex = 0;

        render::DescriptorHeapSet m_descriptorHeapSet{};
        resources::DeviceAllocator m_allocator{};

        FenceWaiter m_fenceWaiter{};

        static constexpr uint32_t m_frameCount = 2U;
        vk::UniqueCommandPool m_uniqueGraphicsCommandPool{};
        render::CommonResources m_commonResources{};
        render::FallbackTextures m_fallbackTextures{};

        SlotMapStorage<render::UploadSceneOutput> m_uploadedScenes{};
        SlotMapStorage<render::DeviceEnvironmentResources> m_environmentResources{};
        SlotMapStorage<FrameResources> m_frameResources{};
        SlotMapStorage<ViewportResources> m_viewportResources{};
        SlotMapStorage<SwapchainResources> m_swapchainResources{};
        SlotMapStorage<vk::UniqueSurfaceKHR> m_surfaces{};
        SlotMapStorage<ImUiContext> m_imGuiContexts{};

        vk::UniquePipelineLayout m_pipelineLayout{};
        FrameGraph frameGraph{};

        std::vector<vk::UniqueCommandBuffer> m_uniqueGraphicsCommandBuffers{};

        [[nodiscard]] vk::ResultValue<vk::UniqueShaderModule> createShaderModuleUnique(std::filesystem::path const &shaderPath) const;

        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createComputePipeline(
            std::filesystem::path const &shaderPath);

        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createMainRenderPipeline(
            vk::Format colorFormat,
            vk::Format depthFormat,
            bool enableDebug
        ) const;

        [[nodiscard]] vk::ResultValue<vk::UniquePipeline> createSkyboxPipeline(
            vk::Format colorFormat,
            vk::Format depthFormat,
            bool enableDebug = false
        ) const;

        [[nodiscard]] vk::Result initFallbackTextures();

        void writeCopyBufferCommand(
            vk::CommandBuffer commandBuffer,
            vk::Buffer stagingBuffer,
            std::vector<CopyBufferCommandInfo> const& copyBufferInfos
        ) const;

        void writeImageSourceBarrier(vk::CommandBuffer commandBuffer, std::vector<ImageBarrierInfo> const& textures) const;

        void writeBufferAndImageDestinationBarrier(
            vk::CommandBuffer commandBuffer,
            std::vector<ImageBarrierInfo> const& textures,
            std::vector<BufferBarrierInfo> const& buffers
        ) const;

        void writeCopyBufferImageCommands(
            vk::CommandBuffer commandBuffer,
            vk::Buffer stagingBuffer,
            const std::vector<vk::Image>& images,
            std::vector<ImageMipUploadInfo> const& textureMipUploadInfos
        ) const;

        [[nodiscard]] vk::ResultValue<vk::UniqueCommandBuffer> allocateOneTimeCommandBuffer();
        [[nodiscard]] vk::ResultValue<render::Texture> createTexture(
            vk::Format format,
            vk::ImageViewType imageViewType,
            vk::Extent3D size,
            uint32_t mipLevels,
            uint32_t arrayLayers
        );

        [[nodiscard]] vk::ResultValue<resources::UniqueAllocatedBuffer> createShaderDeviceAddressBuffer(vk::DeviceSize size);
        [[nodiscard]] vk::ResultValue<resources::UniqueAllocatedBuffer> createStagingBuffer(vk::DeviceSize size);
        [[nodiscard]] vk::ResultValue<resources::UniqueAllocatedBuffer> createIndexBuffer(vk::DeviceSize size);

        [[nodiscard]] vk::Result submitTransferAndWait(vk::CommandBuffer commandBuffer) const;

        [[nodiscard]] vk::Result getFenceStatus(vk::Fence fence) const;

        [[nodiscard]] vk::Result waitForAnyFence(std::initializer_list<vk::Fence> fences, uint64_t timeout = UINT64_MAX) const;
        [[nodiscard]] vk::Result waitForAllFences(std::initializer_list<vk::Fence> fences, uint64_t timeout = UINT64_MAX) const;

        [[nodiscard]] vk::Result waitForAnyFence(std::vector<vk::Fence> const &fences, uint64_t timeout = UINT64_MAX) const;
        [[nodiscard]] vk::Result waitForAllFences(std::vector<vk::Fence> const &fences, uint64_t timeout = UINT64_MAX) const;

        vk::ResultValue<render::CommonResources> createCommonResources(std::filesystem::path const &brdfLutPath);
    };
}

extern "C" {
shuttle::engine::IEngineImpl* createEngineImpl() {
    auto [createResult, engineImpl] = shuttle::engine::EngineImpl::create();
    if (createResult != vk::Result::eSuccess) {
        return nullptr;
    }
    return engineImpl;
}
}

#endif //SHUTTLEENGINE_ENGINE_HPP
