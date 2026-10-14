//
// Created by Shagu on 07.10.2026.
//

#include "DedicatedCommandPool.hpp"

#include <memory>

namespace shuttle::engine {
    DedicatedCommandPool::DedicatedCommandPool(MiUniquePtr<CommandPool>&& pool) noexcept: m_pool{std::move(pool)} {}

    vk::ResultValue<DedicatedCommandPool> DedicatedCommandPool::create(vk::Device device,
        uint32_t queueFamily, const vk::detail::DispatchLoaderDynamic *disp) {

        auto [poolCreateResult, pool] = CommandPool::create(device, queueFamily, disp);
        if (poolCreateResult != vk::Result::eSuccess) {
            return {poolCreateResult, DedicatedCommandPool{}};
        }
        return {vk::Result::eSuccess, DedicatedCommandPool{std::move(pool)}};
    }

    vk::Result DedicatedCommandPool::reset() noexcept {
        assert(m_pool && "Dedicated pool is not initialized!");
        // Перед вызовом reset мы обязаны убедиться через FenceWaiter,
        // что GPU полностью отпустил все буферы, которые мы нарезали из этого пула!
        return m_pool->reset();
    }
}
