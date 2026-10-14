//
// Created by Shagu on 13.09.2026.
//

#include "ImUiContext/ImUiContext.hpp"

#include "EngineImpl.hpp"

namespace shuttle::engine {

    vk::ResultValue<ImUiContextHandle> EngineImpl::createImUiContext(
        pal::WindowBase& window,
        SwapchainResourcesHandle swapchain
    ) {

        auto imageCount = m_swapchainResources.get(swapchain)->imageCount;

        auto [createImUiContextResult, imUiContext] = ImUiContext::create(
            window,
            *m_uniqueInstance,
            m_physicalDevice,
            *m_uniqueDevice,
            m_dispatcher,
            m_graphicsQueueFamilyIndex,
            m_graphicsQueue,
            imageCount
        );

        if (createImUiContextResult != vk::Result::eSuccess) {
            return {createImUiContextResult, {}};
        }

        auto handle = m_imGuiContexts.allocate(std::move(imUiContext));

        return { vk::Result::eSuccess, handle };
    }
}
