//
// Created by Shagu on 05.10.2026.
//

#include "CommandPool.hpp"

#include "MiAllocator/MiAllocator.hpp"

namespace shuttle::engine {
    CommandPool::CommandPool(vk::Device device, vk::detail::DispatchLoaderDynamic const *disp) noexcept: m_device(device), m_dispatcher(disp) {}

    vk::ResultValue<MiUniquePtr<CommandPool>> CommandPool::create(
        vk::Device device,
        uint32_t queueFamilyIndex,
        vk::detail::DispatchLoaderDynamic const *disp)
    {
        auto pool = mi_make_unique_nothrow<CommandPool>(device, disp);
        auto result = pool->init(queueFamilyIndex);
        if (result != vk::Result::eSuccess) {
            return {result, nullptr};
        }
        return {vk::Result::eSuccess, std::move(pool)};
    }

    vk::ResultValue<CommandPool *> CommandPool::inPlaceCreate(
        vk::Device device, uint32_t queueFamilyIndex,
        vk::detail::DispatchLoaderDynamic const *disp,
        void *rawMemory)
    {
        // 1. Вызываем C++ конструктор на месте (Placement New)
        auto* newPool = ::new (rawMemory) CommandPool(device, disp);
        auto result = newPool->init(queueFamilyIndex);
        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }
        return {vk::Result::eSuccess, newPool};
    }

    vk::Result CommandPool::init(uint32_t queueFamilyIndex) {
        auto [result, commandPool] = m_device.createCommandPoolUnique(
        {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilyIndex
        }, nullptr, *m_dispatcher);

        if (result != vk::Result::eSuccess) {
            return result;
        }
        m_commandPool = std::move(commandPool);
        return vk::Result::eSuccess;
    }

    vk::Result CommandPool::reset() noexcept {
        if (!m_commandPool) [[unlikely]] {
            return vk::Result::eSuccess;
        }

        // 1. АППАРАТНЫЙ СБРОС НА GPU: Стираем всю память команд пула у драйвера видеокарты.
        // Обратите внимание: мы убрали const у метода reset(), так как состояние объекта МЕНЯЕТСЯ!
        vk::Result result = m_device.resetCommandPool(m_commandPool.get(), vk::CommandPoolResetFlags{}, *m_dispatcher);

        if (result != vk::Result::eSuccess) [[unlikely]] {
            return result; // Если сам Vulkan не смог сбросить пул — прерываемся
        }

        // 2. ТОТАЛЬНАЯ ЗАЧИСТКА ХОСТА: Выбрасываем все протухшие хэндлы из lock-free очередей!
        // Этот цикл работает со скоростью мысли и полностью очищает "карман" первичных буферов.
        vk::CommandBuffer trashBuffer;
        while (m_freeCommandBuffers.try_dequeue(trashBuffer)) {
            // Мы просто выдергиваем хэндлы из очереди и "сжигаем" их.
            // Вызывать vkFreeCommandBuffers НЕ НУЖНО, так как resetCommandPool уже стер их на GPU!
        }

        // Точно так же очищаем лок-фри очередь вторичных командных буферов
        while (m_freeSecondaryCommandBuffers.try_dequeue(trashBuffer)) {
            // Очищаем
        }

        return vk::Result::eSuccess;
    }


    vk::ResultValue<vk::CommandBuffer> CommandPool::getOrCreateCommandBuffer() {
        vk::CommandBuffer commandBuffer;
        if (!m_freeCommandBuffers.try_dequeue(commandBuffer)) {
            vk::CommandBufferAllocateInfo allocateInfo{
                .commandPool = *m_commandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1
            };
            auto [allocateCommandBufferResult, commandBuffers] = m_device.allocateCommandBuffers(allocateInfo, *m_dispatcher);
            if (allocateCommandBufferResult != vk::Result::eSuccess) {
                return {allocateCommandBufferResult, {}};
            }
            commandBuffer = commandBuffers[0];
        }
        return {vk::Result::eSuccess, commandBuffer};
    }

    CommandPool::~CommandPool() {
        vk::CommandBuffer dummy;
        while (m_freeCommandBuffers.try_dequeue(dummy)) {}
        while (m_freeSecondaryCommandBuffers.try_dequeue(dummy)) {}
    }
}
