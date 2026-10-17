//
// Created by Shagu on 07.10.2026.
//

#ifndef SHUTTLEENGINE_DEDICATED_COMMAND_POOL_HPP
#define SHUTTLEENGINE_DEDICATED_COMMAND_POOL_HPP
#include "include_vulkan.hpp"
#include "command_pool.hpp"
#include <async/mi_allocator.hpp>

namespace shuttle::engine {
    using async::MiUniquePtr;

    class DedicatedCommandPool {
    public:
        // Запрещаем случайное копирование и перемещение аппаратного пула
        DedicatedCommandPool() noexcept = default;
        DedicatedCommandPool(const DedicatedCommandPool&) = delete;
        DedicatedCommandPool& operator=(const DedicatedCommandPool&) = delete;
        DedicatedCommandPool(DedicatedCommandPool&&) noexcept = default;
        DedicatedCommandPool& operator=(DedicatedCommandPool&&) noexcept = default;

        ~DedicatedCommandPool() noexcept = default;

        [[nodiscard]] static vk::ResultValue<DedicatedCommandPool> create(
            vk::Device device, uint32_t queueFamily, const vk::detail::DispatchLoaderDynamic* disp);

        template <typename Func>
        vk::ResultValue<CommandBufferToken> writeCommandBuffer(vk::CommandBufferBeginInfo beginInfo, Func&& func) noexcept {
            return m_pool->writeCommandBuffer(beginInfo, std::forward<Func>(func));
        }

        // Сброс всей памяти пула одним махом (Самая дешевая операция в Vulkan)
        [[nodiscard]] vk::Result reset() noexcept;

    private:
        DedicatedCommandPool(MiUniquePtr<CommandPool>&& pool) noexcept;
        MiUniquePtr<CommandPool> m_pool;
    };
}

#endif //SHUTTLEENGINE_DEDICATED_COMMAND_POOL_HPP
