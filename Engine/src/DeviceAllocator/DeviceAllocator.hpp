#pragma once
#include <iostream>
#include <map>
#include "IncludeVulkan.hpp"
#include <vector>

namespace shuttle::resources
{
enum class AllocationCreateFlagBits : uint32_t
{
    eDedicatedMemory = 0x00000001,
    eNeverAllocate = 0x00000002,
    eMapped = 0x00000004,
    eUserDataCopyString = 0x00000020,
    eUpperAddressBit = 0x00000040,
    eDontBind = 0x00000080,
    eWithinBud = 0x00000100,
    eCanAlias = 0x00000200,
    eHostAccessSequentialWrite = 0x00000400,
    eHostAccessRandom = 0x00000800,
    eStrategyMinMemory = 0x00010000,
    eCreateStrategyMinTime = 0x00020000,
    eCreateStrategyMinOffset = 0x00040000,
    eStrategyBestFit = eStrategyMinMemory,
    eStrategyFirestFit = eCreateStrategyMinTime
};

using AllocationCreateFlags = vk::Flags<AllocationCreateFlagBits>;
} // namespace shuttle::resources

template <> struct vk::FlagTraits<shuttle::resources::AllocationCreateFlagBits>
{
    using WrappedType = uint32_t;

    enum
    {
        isBitmask = true
    };

    static constexpr auto allFlags = shuttle::resources::AllocationCreateFlags(
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eDedicatedMemory) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eNeverAllocate) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eMapped) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eUserDataCopyString) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eUpperAddressBit) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eDontBind) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eWithinBud) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eCanAlias) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eHostAccessRandom) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eStrategyMinMemory) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eCreateStrategyMinTime) |
        static_cast<uint32_t>(shuttle::resources::AllocationCreateFlagBits::eCreateStrategyMinOffset));
};

namespace shuttle::resources
{
using AllocationCreateFlags = vk::Flags<AllocationCreateFlagBits>;

enum class MemoryUsage
{
    eUnknown,
    eGpuOnly,
    eCpuOnly,
    eCpuToGpu,
    eGpuToCpu,
    eAuto,
    eAutoPreferDedicatedMemory,
    ePreferHostMemory,
    ePreferDeviceMemory
};

using AllocationHandle = void*;
using AllocatorHandle = void*;
class DeviceAllocator;

template <typename TResource>
    requires std::same_as<TResource, vk::Buffer> || std::same_as<TResource, vk::Image>
class AllocatedResource
{
  public:
    AllocatedResource() = default;

    AllocatedResource(AllocationHandle const& allocation, TResource resourceHandle)
        : allocation{allocation}, resourceHandle{resourceHandle}
    {
    }

    [[nodiscard]] AllocationHandle getAllocation() const { return allocation; }

    bool operator==(AllocatedResource const& other) const
    {
        return allocation == other.allocation && resourceHandle == other.resourceHandle;
    }

    bool operator!=(AllocatedResource const& other) const { return !(*this == other); }

    operator TResource() const { return resourceHandle; }

  private:
    AllocationHandle allocation = nullptr;
    TResource resourceHandle = VK_NULL_HANDLE;
};

using AllocatedBuffer = AllocatedResource<vk::Buffer>;
using AllocatedImage = AllocatedResource<vk::Image>;

struct StrideCopyHostToBufferInfo
{
    AllocatedBuffer dstBuffer;
    vk::DeviceSize dstBufferOffset{};
    size_t dstBufferStride{0};
    void const* srcData{};
    size_t srcDataStride{0};
    size_t elementCount{};
    size_t elementSize{};
};

struct StrideCopyBufferToHostInfo
{
    AllocatedBuffer srcBuffer;
    vk::DeviceSize srcBufferOffset{};
    size_t srcBufferStride{0};
    void* dstData{};
    size_t dstDataStride{0};
    size_t elementCount{};
    size_t elementSize{};
};

struct CopyHostToBufferInfo
{
    AllocatedBuffer dstBuffer;
    vk::DeviceSize dstBufferOffset{};
    const void* srcData{};
    size_t dataSize{0};
};

struct CopyBufferToHostInfo
{
    AllocatedBuffer srcBuffer;
    vk::DeviceSize srcBufferOffset{};
    void* dstData{};
    size_t dataSize{0};
};

class UniqueAllocatedBufferDeleter
{
  public:
    UniqueAllocatedBufferDeleter(DeviceAllocator const& allocator) noexcept : allocator{&allocator} {}

    UniqueAllocatedBufferDeleter() = default;

    void operator()(AllocatedBuffer const& allocatedBuffer) const noexcept;

  private:
    DeviceAllocator const* allocator = nullptr;
};

class UniqueAllocatedImageDeleter
{
  public:
    UniqueAllocatedImageDeleter(DeviceAllocator const& allocator) noexcept : allocator{&allocator} {}

    UniqueAllocatedImageDeleter() = default;

    void operator()(AllocatedImage const& allocatedImage) const noexcept;

  private:
    DeviceAllocator const* allocator = nullptr;
};

template <typename TResource, typename TDeleter>
    requires std::same_as<TResource, vk::Buffer> || std::same_as<TResource, vk::Image>
class UniqueAllocatedResource
{
public:
    UniqueAllocatedResource() = default;

    UniqueAllocatedResource(
        AllocatedResource<TResource> allocatedResource,
        TDeleter deleter) noexcept
        :
        allocatedResource{allocatedResource},
        deleter{deleter}
    {
    }

    UniqueAllocatedResource(UniqueAllocatedResource const&) = delete;
    UniqueAllocatedResource& operator=(UniqueAllocatedResource const&) = delete;

    UniqueAllocatedResource(UniqueAllocatedResource&& other) noexcept
        :
        allocatedResource{other.allocatedResource},
        deleter{other.deleter}
    {
        other.allocatedResource = AllocatedResource<TResource>{};
        other.deleter = TDeleter{};
    }

    UniqueAllocatedResource& operator=(UniqueAllocatedResource&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        reset();

        allocatedResource = other.allocatedResource;
        deleter = other.deleter;

        other.allocatedResource = AllocatedResource<TResource>{};
        other.deleter = TDeleter{};

        return *this;
    }

    ~UniqueAllocatedResource()
    {
        reset();
    }

    void reset() noexcept
    {
        if (allocatedResource != AllocatedResource<TResource>{})
        {
            deleter(allocatedResource);

            allocatedResource = AllocatedResource<TResource>{};
            deleter = TDeleter{};
        }
    }

    [[nodiscard]]
    AllocatedResource<TResource> release() noexcept
    {
        AllocatedResource<TResource> released =
            allocatedResource;

        allocatedResource =
            AllocatedResource<TResource>{};

        deleter =
            TDeleter{};

        return released;
    }

    [[nodiscard]]
    AllocatedResource<TResource> const& get() const noexcept
    {
        return allocatedResource;
    }

    [[nodiscard]]
    AllocatedResource<TResource>& get() noexcept
    {
        return allocatedResource;
    }

    AllocatedResource<TResource>* operator->() noexcept
    {
        return &allocatedResource;
    }

    AllocatedResource<TResource> const* operator->() const noexcept
    {
        return &allocatedResource;
    }

    AllocatedResource<TResource>& operator*() noexcept
    {
        return allocatedResource;
    }

    AllocatedResource<TResource> const& operator*() const noexcept
    {
        return allocatedResource;
    }

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return allocatedResource != AllocatedResource<TResource>{};
    }

private:
    AllocatedResource<TResource> allocatedResource =
        AllocatedResource<TResource>{};

    TDeleter deleter{};
};

using UniqueAllocatedBuffer = UniqueAllocatedResource<vk::Buffer, UniqueAllocatedBufferDeleter>;
using UniqueAllocatedImage = UniqueAllocatedResource<vk::Image, UniqueAllocatedImageDeleter>;

class DeviceAllocator
{
  public:
    DeviceAllocator() = default;

    DeviceAllocator(AllocatorHandle handle) noexcept : handle{handle} {}

    bool operator==(DeviceAllocator const& other) const noexcept { return handle == other.handle; }

    [[nodiscard]] static vk::ResultValue<DeviceAllocator>
    create(vk::Instance instance, vk::Device device, vk::PhysicalDevice physicalDevice,
           vk::detail::DispatchLoaderDynamic const& dispatcher = vk::detail::defaultDispatchLoaderDynamic) noexcept;

    [[nodiscard]] vk::ResultValue<AllocatedBuffer>
    createAndAllocateBuffer(vk::BufferCreateInfo const& bufferCreateInfo,
                            MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
                            AllocationCreateFlags allocationCreateFlags = {}) const noexcept;

    [[nodiscard]] vk::ResultValue<AllocatedImage>
    createAndAllocateImage(vk::ImageCreateInfo const& imageCreateInfo,
                           MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
                           AllocationCreateFlags allocationCreateFlags = {}) const noexcept;

    [[nodiscard]] vk::ResultValue<UniqueAllocatedBuffer>
    createAndAllocateBufferUnique(vk::BufferCreateInfo const& bufferCreateInfo,
                                  MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
                                  AllocationCreateFlags allocationCreateFlags = {}) const noexcept;

    [[nodiscard]] vk::ResultValue<UniqueAllocatedImage>
    createAndAllocateImageUnique(vk::ImageCreateInfo const& imageCreateInfo,
                                 MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
                                 AllocationCreateFlags allocationCreateFlags = {}) const noexcept;

    [[nodiscard]] vk::Result writeBufferFromHostStride(StrideCopyHostToBufferInfo const& writeInfo) const noexcept;
    [[nodiscard]] vk::Result readBufferToHostStride(StrideCopyBufferToHostInfo const& readInfos) const noexcept;
    [[nodiscard]] vk::Result writeBufferFromHost(CopyHostToBufferInfo const& writeInfo) const noexcept;
    [[nodiscard]] vk::Result readBufferToHost(CopyBufferToHostInfo const& readInfo) const noexcept;
    [[nodiscard]] void* getMappedPointer(AllocatedBuffer buffer) const noexcept;

    DeviceAllocator& operator=(DeviceAllocator const& other) = default;

    void destroyBuffer(AllocatedBuffer allocatedBuffer) const noexcept;

    void destroyImage(AllocatedImage allocatedImage) const noexcept;

    void destroy() const noexcept;

  private:
    AllocatorHandle handle = nullptr;
};

class UniqueAllocator
{
  public:
    UniqueAllocator(DeviceAllocator const& allocator) : allocator{allocator} {}

    UniqueAllocator() = default;
    [[nodiscard]] static vk::ResultValue<UniqueAllocator>
    makeUnique(vk::Instance instance, vk::Device device, vk::PhysicalDevice physicalDevice,
               vk::detail::DispatchLoaderDynamic const& dispatcher = vk::detail::defaultDispatchLoaderDynamic) noexcept;

    UniqueAllocator(UniqueAllocator const&) = delete;
    UniqueAllocator& operator=(UniqueAllocator const&) = delete;

    UniqueAllocator(UniqueAllocator&& other) noexcept : allocator{other.allocator}
    {
        other.allocator = DeviceAllocator{};
    }
    UniqueAllocator& operator=(UniqueAllocator&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (allocator != DeviceAllocator{}) allocator.destroy();

        allocator = other.allocator;
        other.allocator = DeviceAllocator{};

        return *this;
    }

    [[nodiscard]]

    DeviceAllocator& get() noexcept { return allocator; }

    [[nodiscard]] DeviceAllocator const& get() const noexcept { return allocator; }

    DeviceAllocator* operator->() { return &allocator; }

    DeviceAllocator const* operator->() const noexcept { return &allocator; }

    DeviceAllocator& operator*() noexcept { return allocator; }

    DeviceAllocator const& operator*() const noexcept { return allocator; }

    ~UniqueAllocator()
    {
        if (allocator != DeviceAllocator{}) allocator.destroy();
    }

  private:
    DeviceAllocator allocator;
};
} // namespace shuttle::resources
