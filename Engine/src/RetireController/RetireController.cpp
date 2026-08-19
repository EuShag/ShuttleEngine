//
// Created by Shagu on 17.06.2026.
//

#include "RetireController.hpp"

#include <iostream>

namespace shuttle
{

vk::ResultValue<SwapchainResources> RetireController::updateSwapchainResources(
    SwapchainContext const& swapchainContext, vk::Extent2D const& swapchainExtent, uint32_t frameCount, SwapchainResources&& oldSwapchainResources)
{

    auto [createNewSwapchainResult, newSwapchain] =
        createSwapchain(swapchainContext, swapchainExtent, *oldSwapchainResources.swapchain.swapchain);
    if (createNewSwapchainResult != vk::Result::eSuccess) return {createNewSwapchainResult, {}};
    auto [createNewFrameManagerResult, newFrameManager] = FrameManager::create(
        swapchainContext.device, frameCount, newSwapchain.imageCount, std::move(oldSwapchainResources.frameManager));
    if (createNewFrameManagerResult != vk::Result::eSuccess) return {createNewFrameManagerResult, {}};

    // Вычисляем целевые битовые маски (заполняем биты единицами)
    uint32_t targetRender = (1U << frameCount) - 1U;
    uint32_t targetPresent = (1U << oldSwapchainResources.swapchain.imageCount) - 1U;

    retiredSwapchains.emplace_back(std::move(oldSwapchainResources.swapchain.swapchain),
                                   std::move(oldSwapchainResources.frameManager),
                                   0U,           // renderMask (начинаем с нуля, биты будут загораться по ходу кадра)
                                   0U,           // presentMask (начинаем с нуля)
                                   targetRender, // Ожидаемая маска кадров GPU (targetRenderMask)
                                   targetPresent // Ожидаемая маска картинок ОС (targetPresentMask)
    );

    return {vk::Result::eSuccess,
            {
                .swapchain = std::move(newSwapchain),
                .frameManager = std::move(newFrameManager),
            }};
}

void RetireController::renderRetireUpdate(uint32_t frameIndex)
{
    // Проходим по всем старым свопчейнам "на пенсии"
    for (auto& retired : retiredSwapchains)
    {
        // Поджигаем нужный бит в маске рендеринга.
        // Например, если пришел frameIndex = 1, то (1U << 1) даст 0b10.
        // Мы делаем побитовое ИЛИ (|=), добавляя этот бит в текущую маску renderMask.
        retired.renderMask |= (1U << frameIndex);
    }

    // После каждого обновления проверяем, не пора ли кого-то удалить
    cleanupExpired();
}

void RetireController::presentRetireUpdate(uint32_t imageIndex)
{
    // Проходим по всем старым свопчейнам "на пенсии"
    for (auto& retired : retiredSwapchains)
    {
        // Поджигаем нужный бит в маске презентации.
        // Например, если пришел imageIndex = 2, то (1U << 2) даст 0b100.
        // Мы делаем побитовое ИЛИ (|=), добавляя этот бит в текущую маску presentMask.
        retired.presentMask |= (1U << imageIndex);
    }

    // После каждого обновления проверяем, не пора ли кого-то удалить
    cleanupExpired();
}

void RetireController::cleanupExpired()
{
    // Используем C++20 std::erase_if для удаления элементов прямо "на лету"
    std::erase_if(
        retiredSwapchains,
        [](const RetiredSwapchain& retired)
        {
            // Ресурс безопасен для физического удаления ТОЛЬКО тогда, когда:
            // 1) Все биты кадров в полете на GPU отработали (renderMask догнал целевой targetRenderMask)
            // 2) Все биты изображений старого свопчейна освободились в ОС (presentMask догнал targetPresentMask)
            const bool isSafeToDelete =
                retired.renderMask == retired.targetRenderMask && retired.presentMask == retired.targetPresentMask;

            if (isSafeToDelete)
            {
                std::cout << "[System] Retired swapchain resources successfully and safely destroyed on GPU & CPU!\n";
            }
            return isSafeToDelete;
        });
}

} // namespace shuttle
