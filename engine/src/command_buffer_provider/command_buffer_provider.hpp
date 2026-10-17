//
// Created by Shagu on 05.10.2026.
//

#ifndef SHUTTLEENGINE_COMMAND_BUFFER_PROVIDER_HPP
#define SHUTTLEENGINE_COMMAND_BUFFER_PROVIDER_HPP

#include "command_buffer_recorder.hpp"
#include "command_buffer_token.hpp"
#include "include_vulkan.hpp"
#include "concurrentqueue.h"
#include "command_pool.hpp"

#include <array>
#include <cstddef>

namespace shuttle::engine {

    class CommandPoolStorage {
    public:
        static constexpr size_t CAPACITY = 128;

    private:
        // 1. СЫРОЙ БУФЕР: Выровнен монолитно строго под размеры CommandPool
        alignas(CommandPool) std::array<std::byte, CAPACITY * sizeof(CommandPool)> m_storageBuffer{};

        std::atomic<std::size_t> m_allocatedCount{0};
        moodycamel::ConcurrentQueue<CommandPool*> m_freePools{};

        // Vulkan-данные, необходимые для ленивой инициализации новых пулов
        vk::Device m_device;
        uint32_t m_graphicsQueueFamilyIndex;
        const vk::detail::DispatchLoaderDynamic* m_dispatcher{nullptr};

    public:
        CommandPoolStorage(vk::Device device, uint32_t queueFamily, const vk::detail::DispatchLoaderDynamic* disp) noexcept;

        // ЗАПРЕЩАЕМ КОПИРОВАНИЕ МАССИВА ПАМЯТИ
        CommandPoolStorage(const CommandPoolStorage&) = delete;
        CommandPoolStorage& operator=(const CommandPoolStorage&) = delete;

        // АВТО-ДЕСТРУКТОР: Гарантированно зачищает только то, что реально было рождено
        ~CommandPoolStorage() {
            std::size_t const totalAllocated = m_allocatedCount.load(std::memory_order_relaxed);
            for (std::size_t i = 0; i < totalAllocated; ++i) {
                auto* pool = reinterpret_cast<CommandPool*>(&m_storageBuffer[i * sizeof(CommandPool)]);
                pool->~CommandPool(); // Очереди и vk::UniqueCommandPool внутри CommandPool закроются сами!
            }
        }

        [[nodiscard]] vk::Result init(uint32_t initialCount) noexcept;

        // --- МОНОЛИТНЫЙ ЗАХВАТ С АВТО-ИНИЦИАЛИЗАЦИЕЙ VULKAN ---
        [[nodiscard]] vk::ResultValue<CommandPool*> acquire() noexcept;

        // Возврат пула обратно на склад
        void recycle(CommandPool* pool) noexcept;
    };

    class CommandBufferProvider {

        class CommandPoolGuard {
        public:
            CommandPoolGuard(CommandPool* pool, CommandPoolStorage* storage)
                : m_pool(pool), m_storage(storage) {}

            ~CommandPoolGuard() {
                if (m_pool && m_storage) {
                    m_storage->recycle(m_pool);
                }
            }

            CommandPool& operator*() const {
                return *m_pool;
            }
            CommandPool& operator*() {
                return *m_pool;
            }
            CommandPool* operator->() {
                return m_pool;
            }
            CommandPool const* operator->() const {
                return m_pool;
            }

        private:
            CommandPool* m_pool{};
            CommandPoolStorage* m_storage{};
        };

    public:
        CommandBufferProvider(CommandBufferProvider const&) = delete;
        CommandBufferProvider& operator=(CommandBufferProvider const&) = delete;

        CommandBufferProvider(vk::Device device, uint32_t queueFamily, vk::detail::DispatchLoaderDynamic const *disp) noexcept;

        [[nodiscard]] static vk::ResultValue<MiUniquePtr<CommandBufferProvider>> create(
            vk::Device device, uint32_t queueFamily, vk::detail::DispatchLoaderDynamic const *disp,
            uint32_t initialCount);
        [[nodiscard]] static vk::ResultValue<CommandBufferProvider*> createInPlace(
            vk::Device device, uint32_t queueFamily, vk::detail::DispatchLoaderDynamic const *disp,
            void* memory, uint32_t initialCount) noexcept;

        [[nodiscard]] vk::Result init(uint32_t initialCount);

        template<typename Func> requires std::invocable<Func, CommandBufferRecorder const&>
        [[nodiscard]] vk::ResultValue<CommandBufferToken> writeCommandBuffer(vk::CommandBufferBeginInfo beginInfo, Func&& func);

    private:
        CommandPoolStorage m_commandPools{m_device, m_graphicsQueueFamilyIndex, m_dispatcher};
        vk::Device m_device;

        uint32_t m_graphicsQueueFamilyIndex;
        vk::detail::DispatchLoaderDynamic const* m_dispatcher;

        [[nodiscard]] vk::ResultValue<CommandPoolGuard> getOrCreatePool();
    };

    template<typename Func> requires std::invocable<Func, CommandBufferRecorder const &>
    vk::ResultValue<CommandBufferToken> CommandBufferProvider::writeCommandBuffer(vk::CommandBufferBeginInfo beginInfo, Func &&func) {
        // 1. Lock-free вытягиваем УКАЗАТЕЛЬ на свободный пул
        auto [poolPtrResult, poolPtr] = getOrCreatePool();
        if (poolPtrResult != vk::Result::eSuccess) {
            return {poolPtrResult, {}};
        }
        return poolPtr->writeCommandBuffer(beginInfo, std::forward<Func>(func));
    }
}

#endif //SHUTTLEENGINE_COMMAND_BUFFER_PROVIDER_HPP
