#include "VirtualBlock.hpp"

#include "vk_mem_alloc.h"

namespace shuttle::resources
{

vk::ResultValue<VirtualBlock> VirtualBlock::create(
    vk::DeviceSize size) noexcept
{
    VmaVirtualBlockCreateInfo createInfo{
        .size = static_cast<VkDeviceSize>(size)
    };

    VmaVirtualBlock block{};

    const VkResult result =
        vmaCreateVirtualBlock(
            &createInfo,
            &block);

    return {
        static_cast<vk::Result>(result),
        VirtualBlock{
            static_cast<VirtualBlockHandle>(block)
        }
    };
}

vk::ResultValue<VirtualAllocation> VirtualBlock::allocate(
    vk::DeviceSize size,
    vk::DeviceSize alignment) const noexcept
{
    VmaVirtualAllocationCreateInfo allocationCreateInfo{
        .size = static_cast<VkDeviceSize>(size),
        .alignment = static_cast<VkDeviceSize>(alignment)
    };

    VmaVirtualAllocation allocation{};
    VkDeviceSize offset{};

    const VkResult result =
        vmaVirtualAllocate(
            static_cast<VmaVirtualBlock>(handle),
            &allocationCreateInfo,
            &allocation,
            &offset);

    return {
        static_cast<vk::Result>(result),
        VirtualAllocation{
            .allocation =
                static_cast<VirtualAllocationHandle>(allocation),

            .offset =
                static_cast<vk::DeviceSize>(offset),

            .size =
                size
        }
    };
}

void VirtualBlock::free(
    VirtualAllocation allocation) const noexcept
{
    if (handle == nullptr || !allocation)
    {
        return;
    }

    vmaVirtualFree(
        static_cast<VmaVirtualBlock>(handle),
        static_cast<VmaVirtualAllocation>(
            allocation.allocation));
}

void VirtualBlock::clear() const noexcept
{
    if (handle == nullptr)
    {
        return;
    }

    vmaClearVirtualBlock(
        static_cast<VmaVirtualBlock>(handle));
}

void VirtualBlock::destroy() const noexcept
{
    if (handle == nullptr)
    {
        return;
    }

    vmaDestroyVirtualBlock(
        static_cast<VmaVirtualBlock>(handle));
}

vk::ResultValue<UniqueVirtualBlock> UniqueVirtualBlock::makeUnique(
    vk::DeviceSize size) noexcept
{
    auto result =
        VirtualBlock::create(size);

    if (result.result != vk::Result::eSuccess)
    {
        return {
            result.result,
            UniqueVirtualBlock{}
        };
    }

    return {
        result.result,
        UniqueVirtualBlock{
            result.value
        }
    };
}

} // namespace shuttle::resources