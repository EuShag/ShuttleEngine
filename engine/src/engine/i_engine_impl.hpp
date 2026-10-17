//
// Created by Shagu on 01.10.2026.
//

#ifndef SHUTTLEENGINE_IENGINEIMPL_HPP
#define SHUTTLEENGINE_IENGINEIMPL_HPP
#include "handle.hpp"
#include "camera/camera.hpp"
#include "environment_data_loader/environment_data_loader.hpp"
#include "pal/common/window/window_base.hpp"
#include "render/main_pass.hpp"
#include "scene_data_loader/scene_data_loader.hpp"

namespace shuttle {
    struct IUiPainter;
}

namespace shuttle::engine {

    struct SceneHandleTag {};
    struct EnvironmentHandleTag {};
    struct FrameResourcesHandleTag {};
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
    using ViewportResourcesHandle  = TypedHandle<ViewportResourcesHandleTag>;
    using SwapchainResourcesHandle = TypedHandle<SwapchainResourcesHandleTag>;
    using SurfaceHandle            = TypedHandle<SurfaceHandleTag>;
    using ImUiContextHandle        = TypedHandle<ImUiContextHandleTag>;


    class IEngineImpl
    {
    public:
        virtual ~IEngineImpl() = default;

        struct RenderIndices
        {
            uint32_t frameIndex = 0;
            uint32_t imageIndex = 0;
        };

        [[nodiscard]] virtual vk::ResultValue<EnvironmentHandle> createEnvironmentResources(
            render::LoadedEnvironmentData const &data) = 0;

        [[nodiscard]] virtual vk::ResultValue<SceneHandle> uploadScene(
            render::LoadedSceneData const &sceneData
        ) = 0;

        [[nodiscard]] virtual vk::ResultValue<FrameResourcesHandle> createFrameResources(
            SceneHandle sceneHandle,
            FrameResourcesHandle oldResources) = 0;

        [[nodiscard]] virtual vk::ResultValue<ViewportResourcesHandle> createViewportResources(
            vk::Extent2D viewportExtent) = 0;

        [[nodiscard]] virtual vk::ResultValue<ViewportIndices> getViewportIndices(
            ImUiContextHandle imUiContextHandle,
            ViewportTexturesHandle viewportTexturesHandle,
            uint32_t index) = 0;

        [[nodiscard]] virtual vk::ResultValue<ViewportTexturesHandle> createViewportTextures(
            ImUiContextHandle imUiContextHandle,
            ViewportResourcesHandle viewportResourcesHandle) = 0;

        [[nodiscard]] virtual vk::ResultValue<SurfaceHandle> createSurface(
            pal::Platform const &platform,
            pal::WindowHandle const &window) = 0;

        [[nodiscard]] virtual vk::ResultValue<SwapchainResourcesHandle> createSwapchainResources(
            SurfaceHandle surfaceHandle,
            vk::Extent2D extent,
            SwapchainResourcesHandle oldSwapchainResourcesHandle) = 0;

        [[nodiscard]] virtual vk::ResultValue<ImUiContextHandle> createImUiContext(
            pal::WindowBase& window,
            SwapchainResourcesHandle swapchainResources) = 0;

        virtual vk::Result renderUiOnly(
            IUiPainter &uiPainter,
            ImUiContextHandle imUiContextHandle,
            FrameResourcesHandle frameResourcesHandle,
            ViewportResourcesHandle viewportResourcesHandle,
            ViewportTexturesHandle viewportTexturesHandle,
            SwapchainResourcesHandle swapchainResourcesHandle
        ) = 0;

        virtual vk::Result renderSceneUi(
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
        ) = 0;

        virtual void destroyScene(SceneHandle sceneHandle) = 0;
        virtual void destroyEnvironment(EnvironmentHandle environmentHandle) = 0;
        virtual void destroyFrameResources(FrameResourcesHandle frameResourcesHandle) = 0;
        virtual void destroyViewportResources(ViewportResourcesHandle viewportResourcesHandle) = 0;
        virtual void destroySurface(SurfaceHandle surfaceHandle) = 0;
        virtual void destroySwapchainResources(SwapchainResourcesHandle swapchainResourcesHandle) = 0;
        virtual void destroyImUiContext(ImUiContextHandle typed_handle) = 0;
    };


}

#endif //SHUTTLEENGINE_IENGINEIMPL_HPP
