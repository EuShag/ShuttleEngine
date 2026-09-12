//
// Created by Shagu on 15.06.2026.
//

#include "FrameManager.hpp"
#include <iostream>

namespace shuttle
{

    vk::ResultValue<FrameManager>
    FrameManager::create(
        vk::Device device,
        uint32_t framesInFlightCount,
        vk::detail::DispatchLoaderDynamic const& dispatcher)
    {
        FrameManager result{framesInFlightCount};
        if (auto initResult = result.init(device, dispatcher); initResult != vk::Result::eSuccess)
        {
            return {initResult, {}};
        }
        return {vk::Result::eSuccess, std::move(result)};
    }

    vk::Result FrameManager::prepareFrameSlot(vk::Device device, uint32_t frameIndex, vk::detail::DispatchLoaderDynamic const& dldi)
    {
        return device.waitForFences({*uniqueInFlightFences[frameIndex]}, true, UINT64_MAX, dldi);
    }

    vk::ResultValue<uint32_t> FrameManager::acquireNextFrameSlot(vk::Device device, vk::detail::DispatchLoaderDynamic const& dldi)
    {
        for (uint32_t frameIndex = 0; frameIndex < framesInFlightCount; ++frameIndex)
        {
            auto fenceStatus = device.getFenceStatus(*uniqueInFlightFences[frameIndex], dldi);
            if (fenceStatus == vk::Result::eSuccess) {
                return {vk::Result::eSuccess, frameIndex};
            }
            if (fenceStatus != vk::Result::eNotReady) {
                return {fenceStatus, 0};
            }
        }
        return {vk::Result::eNotReady, 0};
    }

    vk::Result FrameManager::beginFrame(vk::Device device, uint32_t frameIndex, vk::detail::DispatchLoaderDynamic const& dldi) {
        return device.resetFences({*uniqueInFlightFences[frameIndex]}, dldi);
    }

    vk::ResultValue<uint32_t> FrameManager::acquireNextImage(vk::Device device, vk::SwapchainKHR swapchain, uint32_t frameIndex, vk::detail::DispatchLoaderDynamic const& dldi)
    {
        return device.acquireNextImageKHR(swapchain, UINT64_MAX, *uniqueImageAvailableSemaphores[frameIndex], nullptr, dldi);
    }

    vk::Result FrameManager::waitRenderIdle(vk::Device device, vk::detail::DispatchLoaderDynamic const& dldi) const noexcept {
        std::vector<vk::Fence> fences;
        fences.reserve(uniqueInFlightFences.size());
        for (const auto& fence : uniqueInFlightFences)
        {
            fences.push_back(*fence);
        }
        return device.waitForFences(fences, true, UINT64_MAX, dldi);
    }

    vk::Result FrameManager::init(vk::Device device, vk::detail::DispatchLoaderDynamic const& dldi)
    {
        uniqueInFlightFences.resize(framesInFlightCount);
        uniqueImageAvailableSemaphores.resize(framesInFlightCount);

        for (auto& uniqueInFlightFence : uniqueInFlightFences)
        {
            auto [result, fence] = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}, nullptr, dldi);

            if (result != vk::Result::eSuccess) return result;

            uniqueInFlightFence = std::move(fence);
        }

        for (auto& uniqueImageAvailableSemaphore : uniqueImageAvailableSemaphores)
        {
            auto [result, semaphore] = device.createSemaphoreUnique({}, nullptr, dldi);

            if (result != vk::Result::eSuccess) return result;

            uniqueImageAvailableSemaphore = std::move(semaphore);
        }
        return vk::Result::eSuccess;
    }
} // namespace shuttle