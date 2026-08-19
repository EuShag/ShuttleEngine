#pragma once

#include "IncludeVulkan.hpp"

namespace shuttle::resources
{
using VirtualBlockHandle = void*;
using VirtualAllocationHandle = void*;

struct VirtualAllocation
{
    VirtualAllocationHandle allocation{};
    vk::DeviceSize offset{};
    vk::DeviceSize size{};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return allocation != nullptr;
    }
};

class VirtualBlock
{
public:
    VirtualBlock() = default;

    explicit VirtualBlock(VirtualBlockHandle handle) noexcept
        : handle(handle)
    {
    }

    [[nodiscard]]
    static vk::ResultValue<VirtualBlock> create(
        vk::DeviceSize size) noexcept;

    [[nodiscard]]
    vk::ResultValue<VirtualAllocation> allocate(
        vk::DeviceSize size,
        vk::DeviceSize alignment = 1) const noexcept;

    void free(
        VirtualAllocation allocation) const noexcept;

    void clear() const noexcept;

    void destroy() const noexcept;

    [[nodiscard]]
    bool operator==(VirtualBlock const& other) const noexcept
    {
        return handle == other.handle;
    }

    [[nodiscard]]
    bool operator!=(VirtualBlock const& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]]
    VirtualBlockHandle getHandle() const noexcept
    {
        return handle;
    }

private:
    VirtualBlockHandle handle{};
};

class UniqueVirtualBlock
{
public:
    UniqueVirtualBlock() = default;

    explicit UniqueVirtualBlock(
        VirtualBlock virtualBlock) noexcept
        : virtualBlock(virtualBlock)
    {
    }

    [[nodiscard]]
    static vk::ResultValue<UniqueVirtualBlock> makeUnique(
        vk::DeviceSize size) noexcept;

    UniqueVirtualBlock(UniqueVirtualBlock const&) = delete;

    UniqueVirtualBlock& operator=(
        UniqueVirtualBlock const&) = delete;

    UniqueVirtualBlock(UniqueVirtualBlock&& other) noexcept
        : virtualBlock(other.virtualBlock)
    {
        other.virtualBlock = VirtualBlock{};
    }

    UniqueVirtualBlock& operator=(
        UniqueVirtualBlock&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (virtualBlock != VirtualBlock{})
        {
            virtualBlock.destroy();
        }

        virtualBlock = other.virtualBlock;
        other.virtualBlock = VirtualBlock{};

        return *this;
    }

    [[nodiscard]]
    VirtualBlock& get() noexcept
    {
        return virtualBlock;
    }

    [[nodiscard]]
    VirtualBlock const& get() const noexcept
    {
        return virtualBlock;
    }

    [[nodiscard]]
    VirtualBlock* operator->() noexcept
    {
        return &virtualBlock;
    }

    [[nodiscard]]
    VirtualBlock const* operator->() const noexcept
    {
        return &virtualBlock;
    }

    [[nodiscard]]
    VirtualBlock& operator*() noexcept
    {
        return virtualBlock;
    }

    [[nodiscard]]
    VirtualBlock const& operator*() const noexcept
    {
        return virtualBlock;
    }

    ~UniqueVirtualBlock()
    {
        if (virtualBlock != VirtualBlock{})
        {
            virtualBlock.destroy();
        }
    }

private:
    VirtualBlock virtualBlock{};
};

} // namespace shuttle::resources