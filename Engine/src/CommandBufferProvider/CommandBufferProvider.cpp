//
// Created by Shagu on 05.10.2026.
//

#include "CommandBufferProvider.hpp"
#include "mimalloc.h"

namespace  {
    template <typename T, typename... Args>
    [[nodiscard]] std::unique_ptr<T> make_unique_nothrow(Args&&... args) noexcept {
        // ЖЕСТКИЙ СТАТИЧЕСКИЙ ПРЕДОХРАНИТЕЛЬ:
        // Если конструктор типа T с заданными аргументами не гарантирует noexcept,
        // компилятор мгновенно остановит сборку движка и выдаст понятную ошибку!
        static_assert(std::is_nothrow_constructible_v<T, Args...>,
            "Critical Error: Type T must be completely noexcept constructible with provided arguments!");

        // 1. Вызываем специальную перегрузку оператора new, которая принимает std::nothrow.
        void* memory = operator new(sizeof(T), std::nothrow);

        // 2. Жестко проверяем, выделилась ли память под объект
        if (!memory) [[unlikely]] {
            return std::unique_ptr<T>(nullptr);
        }

        // 3. Если память есть, конструируем объект прямо по этому адресу (Placement New).
        // Благодаря static_assert выше, мы на 100% уверены, что эта строчка не нарушит noexcept контракт фабрики.
        T* ptr = ::new (memory) T(std::forward<Args>(args)...);

        // 4. Оборачиваем сырой указатель в стандартный std::unique_ptr и возвращаем наружу
        return std::unique_ptr<T>(ptr);
    }
}

namespace shuttle::engine {
    CommandPoolStorage::CommandPoolStorage(vk::Device device, uint32_t queueFamily,
        const vk::detail::DispatchLoaderDynamic *disp) noexcept: m_device(device), m_graphicsQueueFamilyIndex(queueFamily), m_dispatcher(disp) {}

    [[nodiscard]] vk::Result CommandPoolStorage::init(uint32_t initialCount) noexcept {
        if (initialCount == 0) return vk::Result::eSuccess;

        const uint32_t countToWarmup = (std::min)(static_cast<size_t>(initialCount), CAPACITY);

        for (uint32_t i = 0; i < countToWarmup; ++i) {
            void* rawMemory = &m_storageBuffer[i * sizeof(CommandPool)];

            // Бьем напрямую в вашу in-place фабрику!

            if (auto [result, _] = CommandPool::inPlaceCreate(m_device, m_graphicsQueueFamilyIndex, m_dispatcher, rawMemory); result != vk::Result::eSuccess) [[unlikely]] {
                return result;
            }

            m_allocatedCount.store(i + 1, std::memory_order_relaxed);
        }

        return vk::Result::eSuccess;
    }


    [[nodiscard]] vk::ResultValue<CommandPool*> CommandPoolStorage::acquire() noexcept {
        CommandPool* poolPtr = nullptr;

        // Fast-Path: Пул уже когда-то создавался и вернулся в lock-free очередь
        if (m_freePools.try_dequeue(poolPtr)) {
            return {vk::Result::eSuccess, poolPtr};
        }

        // Slow-Path: Очередь пуста, нарезаем новую ячейку в монолитном байтовом буфере
        std::size_t index = m_allocatedCount.fetch_add(1, std::memory_order_relaxed);

        if (index >= CAPACITY) [[unlikely]] {
            return {vk::Result::eErrorOutOfPoolMemory, nullptr};
        }

        // Вычисляем адрес ячейки в сыром буфере
        void* rawMemory = &m_storageBuffer[index * sizeof(CommandPool)];

        // МАГИЯ: Мы просто делегируем всю грязную работу вашей инлайн-фабрике!
        // Она сама сделает placement new, сама вызовет init и вернет результат
        auto [result, newPool] = CommandPool::inPlaceCreate(m_device, m_graphicsQueueFamilyIndex, m_dispatcher, rawMemory);

        if (result != vk::Result::eSuccess) [[unlikely]] {
            // Если Vulkan упал, фабрика внутри себя откатала placement new,
            // а мы просто пробрасываем ошибку наверх
            return {result, nullptr};
        }

        return {vk::Result::eSuccess, newPool};
    }


    void CommandPoolStorage::recycle(CommandPool *pool) noexcept {
        if (pool) [[likely]] {
            m_freePools.enqueue(pool);
        }
    }

    CommandBufferProvider::CommandBufferProvider(
        vk::Device device, uint32_t queueFamily,
        vk::detail::DispatchLoaderDynamic const *disp) noexcept:
    m_device(device), m_graphicsQueueFamilyIndex(queueFamily), m_dispatcher(disp) {}

    vk::ResultValue<MiUniquePtr<CommandBufferProvider>> CommandBufferProvider::create(
        vk::Device device, uint32_t queueFamily,
        vk::detail::DispatchLoaderDynamic const *disp, uint32_t initialCount) {
        auto provider = mi_make_unique_nothrow<CommandBufferProvider>(device, queueFamily, disp);
        if (!provider) {
            return {vk::Result::eErrorOutOfHostMemory, nullptr};
        }
        if (auto result = provider->init(initialCount); result != vk::Result::eSuccess) {
            return {result, nullptr};
        }
        return {vk::Result::eSuccess, std::move(provider)};
    }

    vk::ResultValue<CommandBufferProvider *> CommandBufferProvider::createInPlace(vk::Device device,
        uint32_t queueFamily, vk::detail::DispatchLoaderDynamic const *disp, void *memory,
        uint32_t initialCount) noexcept {
        auto provider = new (memory) CommandBufferProvider(device, queueFamily, disp);
        if (!provider) {
            return {vk::Result::eErrorOutOfHostMemory, nullptr};
        }
        if (auto result = provider->init(initialCount); result != vk::Result::eSuccess) {
            return {result, nullptr};
        }
        return {vk::Result::eSuccess, provider};
    }

    vk::Result CommandBufferProvider::init(uint32_t initialCount) {
        return m_commandPools.init(initialCount);
    }

    vk::ResultValue<CommandBufferProvider::CommandPoolGuard> CommandBufferProvider::getOrCreatePool() {
        auto [result, pool] = m_commandPools.acquire();
        return {result, CommandPoolGuard(pool, &m_commandPools)};
    }

}
