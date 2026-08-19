#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#define VK_NO_PROTOTYPES
#define VMA_IMPLEMENATION
#include "vk_mem_alloc.h"
#include "DeviceAllocator.hpp"

#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle::resources
{

vk::ResultValue<DeviceAllocator> DeviceAllocator::create(vk::Instance instance, vk::Device device,
                                                         vk::PhysicalDevice physicalDevice,
                                                         vk::detail::DispatchLoaderDynamic const& dispatcher) noexcept
{

    VmaAllocator resultAllocator;

    VmaVulkanFunctions vulkanFunctions{.vkGetInstanceProcAddr = dispatcher.vkGetInstanceProcAddr,
                                       .vkGetDeviceProcAddr = dispatcher.vkGetDeviceProcAddr,
                                       .vkGetPhysicalDeviceProperties = dispatcher.vkGetPhysicalDeviceProperties,
                                       .vkGetPhysicalDeviceMemoryProperties =
                                           dispatcher.vkGetPhysicalDeviceMemoryProperties,
                                       .vkAllocateMemory = dispatcher.vkAllocateMemory,
                                       .vkFreeMemory = dispatcher.vkFreeMemory,
                                       .vkMapMemory = dispatcher.vkMapMemory,
                                       .vkUnmapMemory = dispatcher.vkUnmapMemory,
                                       .vkFlushMappedMemoryRanges = dispatcher.vkFlushMappedMemoryRanges,
                                       .vkInvalidateMappedMemoryRanges = dispatcher.vkInvalidateMappedMemoryRanges,
                                       .vkBindBufferMemory = dispatcher.vkBindBufferMemory,
                                       .vkBindImageMemory = dispatcher.vkBindImageMemory,
                                       .vkGetBufferMemoryRequirements = dispatcher.vkGetBufferMemoryRequirements,
                                       .vkGetImageMemoryRequirements = dispatcher.vkGetImageMemoryRequirements,
                                       .vkCreateBuffer = dispatcher.vkCreateBuffer,
                                       .vkDestroyBuffer = dispatcher.vkDestroyBuffer,
                                       .vkCreateImage = dispatcher.vkCreateImage,
                                       .vkDestroyImage = dispatcher.vkDestroyImage};

    VmaAllocatorCreateInfo allocatorCreateInfo{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                               .physicalDevice = physicalDevice,
                                               .device = device,
                                               .preferredLargeHeapBlockSize = 0,
                                               .pAllocationCallbacks = nullptr,
                                               .pDeviceMemoryCallbacks = nullptr,
                                               .pHeapSizeLimit = nullptr,
                                               .pVulkanFunctions = &vulkanFunctions,
                                               .instance = instance,
                                               .vulkanApiVersion = VK_API_VERSION_1_0,
                                               .pTypeExternalMemoryHandleTypes = nullptr};

    auto result = vmaCreateAllocator(&allocatorCreateInfo, &resultAllocator);
    return vk::ResultValue{static_cast<vk::Result>(result), DeviceAllocator{resultAllocator}};
}

vk::ResultValue<AllocatedBuffer>
DeviceAllocator::createAndAllocateBuffer(vk::BufferCreateInfo const& bufferCreateInfo, MemoryUsage desiredMemoryUsage,
                                         AllocationCreateFlags allocationCreateFlags) const noexcept
{
    auto allocationCreateFlagsInt{static_cast<uint32_t>(allocationCreateFlags)};
    auto memoryUsageInt{static_cast<VmaMemoryUsage>(desiredMemoryUsage)};

    VmaAllocationCreateInfo allocationCreateInfo{.flags = allocationCreateFlagsInt,
                                                 .usage = memoryUsageInt,
                                                 .requiredFlags = 0,
                                                 .preferredFlags = 0,
                                                 .memoryTypeBits = 0,
                                                 .pool = nullptr,
                                                 .pUserData = nullptr,
                                                 .priority = 0.0f};

    auto&& allocator = reinterpret_cast<VmaAllocator>(handle);

    VmaAllocation allocation;
    VkBuffer buffer;

    auto result = vmaCreateBuffer(allocator, bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, nullptr);

    return vk::ResultValue{static_cast<vk::Result>(result), AllocatedBuffer{allocation, buffer}};
}

vk::ResultValue<AllocatedImage>
DeviceAllocator::createAndAllocateImage(vk::ImageCreateInfo const& imageCreateInfo, MemoryUsage desiredMemoryUsage,
                                        AllocationCreateFlags allocationCreateFlags) const noexcept
{
    auto allocationCreateFlagsInt{static_cast<uint32_t>(allocationCreateFlags)};
    auto memoryUsageInt{static_cast<VmaMemoryUsage>(desiredMemoryUsage)};
    VmaAllocationCreateInfo allocationCreateInfo{.flags = allocationCreateFlagsInt,
                                                 .usage = memoryUsageInt,
                                                 .requiredFlags = 0,
                                                 .preferredFlags = 0,
                                                 .memoryTypeBits = 0,
                                                 .pool = nullptr,
                                                 .pUserData = nullptr,
                                                 .priority = 0.0f};
    auto&& allocator = static_cast<VmaAllocator>(handle);
    VmaAllocation allocation;
    VkImage image;
    auto result = vmaCreateImage(allocator, imageCreateInfo, &allocationCreateInfo, &image, &allocation, nullptr);
    return vk::ResultValue{static_cast<vk::Result>(result), AllocatedImage{allocation, image}};
}

vk::ResultValue<UniqueAllocatedBuffer>
DeviceAllocator::createAndAllocateBufferUnique(vk::BufferCreateInfo const& bufferCreateInfo,
                                               MemoryUsage desiredMemoryUsage,
                                               AllocationCreateFlags allocationCreateFlags) const noexcept
{
    auto result = createAndAllocateBuffer(bufferCreateInfo, desiredMemoryUsage, allocationCreateFlags);
    if (!result.has_value())
    {
        return vk::ResultValue{result.result, UniqueAllocatedBuffer{}};
    }
    return vk::ResultValue{result.result, UniqueAllocatedBuffer{result.value, UniqueAllocatedBufferDeleter{*this}}};
}

vk::ResultValue<UniqueAllocatedImage>
DeviceAllocator::createAndAllocateImageUnique(vk::ImageCreateInfo const& imageCreateInfo,
                                              MemoryUsage desiredMemoryUsage,
                                              AllocationCreateFlags allocationCreateFlags) const noexcept
{
    auto result = createAndAllocateImage(imageCreateInfo, desiredMemoryUsage, allocationCreateFlags);
    if (!result.has_value())
    {
        return vk::ResultValue{result.result, UniqueAllocatedImage{}};
    }
    return vk::ResultValue{result.result, UniqueAllocatedImage{result.value, UniqueAllocatedImageDeleter{*this}}};
}

vk::Result DeviceAllocator::writeBufferFromHostStride(StrideCopyHostToBufferInfo const& writeInfo) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(writeInfo.dstBuffer.getAllocation());
    void* mappedData = nullptr;
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(allocator, allocationHandle, &allocationInfo);

    if (mappedData = allocationInfo.pMappedData; mappedData != nullptr)
    {
        char* writePtr = static_cast<char*>(mappedData) + writeInfo.dstBufferOffset;
        strideCopy(writePtr, writeInfo.srcData, writeInfo.elementSize, writeInfo.elementCount,
                   writeInfo.dstBufferStride, writeInfo.srcDataStride);
        return vk::Result::eSuccess;
    }

    auto mapResult = vmaMapMemory(allocator, allocationHandle, &mappedData);
    if (mapResult != VK_SUCCESS)
    {
        return static_cast<vk::Result>(mapResult);
    }

    char* writePtr = static_cast<char*>(mappedData) + writeInfo.dstBufferOffset;
    strideCopy(writePtr, writeInfo.srcData, writeInfo.elementSize, writeInfo.elementCount, writeInfo.dstBufferStride,
               writeInfo.srcDataStride);

    vmaUnmapMemory(allocator, allocationHandle);
    return vk::Result::eSuccess;
}

vk::Result DeviceAllocator::writeBufferFromHost(CopyHostToBufferInfo const& writeInfo) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(writeInfo.dstBuffer.getAllocation());
    void* mappedData = nullptr;
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(allocator, allocationHandle, &allocationInfo);

    if (mappedData = allocationInfo.pMappedData; mappedData != nullptr)
    {
        char* writePtr = static_cast<char*>(mappedData) + writeInfo.dstBufferOffset;
        std::memcpy(writePtr, writeInfo.srcData, writeInfo.dataSize);
        return vk::Result::eSuccess;
    }

    auto mapResult = vmaMapMemory(allocator, allocationHandle, &mappedData);
    if (mapResult != VK_SUCCESS)
    {
        return static_cast<vk::Result>(mapResult);
    }

    char* writePtr = static_cast<char*>(mappedData) + writeInfo.dstBufferOffset;
    std::memcpy(writePtr, writeInfo.srcData, writeInfo.dataSize);

    vmaUnmapMemory(allocator, allocationHandle);
    return vk::Result::eSuccess;
}

vk::Result DeviceAllocator::readBufferToHost(CopyBufferToHostInfo const& readInfo) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(readInfo.srcBuffer.getAllocation());
    void* mappedData = nullptr;
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(allocator, allocationHandle, &allocationInfo);
    if (mappedData = allocationInfo.pMappedData; mappedData != nullptr)
    {
        char* readPtr = static_cast<char*>(mappedData) + readInfo.srcBufferOffset;
        std::memcpy(readPtr, readInfo.dstData, readInfo.dataSize);
        return vk::Result::eSuccess;
    }

    auto mapResult = vmaMapMemory(allocator, allocationHandle, &mappedData);
    if (mapResult != VK_SUCCESS)
    {
        return static_cast<vk::Result>(mapResult);
    }

    char* readPtr = static_cast<char*>(mappedData) + readInfo.srcBufferOffset;
    std::memcpy(readPtr, readInfo.dstData, readInfo.dataSize);

    vmaUnmapMemory(allocator, allocationHandle);
    return vk::Result::eSuccess;
}

void* DeviceAllocator::getMappedPointer(AllocatedBuffer buffer) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(buffer.getAllocation());
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(allocator, allocationHandle, &allocationInfo);
    return allocationInfo.pMappedData;
}

vk::Result DeviceAllocator::readBufferToHostStride(StrideCopyBufferToHostInfo const& readInfos) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(readInfos.srcBuffer.getAllocation());
    void* mappedData = nullptr;
    VmaAllocationInfo allocationInfo;

    if (mappedData = allocationInfo.pMappedData; mappedData != nullptr)
    {
        char* readPtr = static_cast<char*>(mappedData) + readInfos.srcBufferOffset;
        strideCopy(readInfos.dstData, readPtr, readInfos.elementSize, readInfos.elementCount, readInfos.dstDataStride,
                   readInfos.srcBufferStride);
        return vk::Result::eSuccess;
    }

    auto mapResult = vmaMapMemory(allocator, allocationHandle, &mappedData);
    if (mapResult != VK_SUCCESS)
    {
        return static_cast<vk::Result>(mapResult);
    }

    char* readPtr = static_cast<char*>(mappedData) + readInfos.srcBufferOffset;
    strideCopy(readInfos.dstData, readPtr, readInfos.elementSize, readInfos.elementCount, readInfos.dstDataStride,
               readInfos.srcBufferStride);

    vmaUnmapMemory(allocator, allocationHandle);
    return vk::Result::eSuccess;
}

void DeviceAllocator::destroyBuffer(AllocatedBuffer buffer) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(buffer.getAllocation());
    auto&& bufferHandle = static_cast<VkBuffer>(static_cast<vk::Buffer>(buffer));
    vmaDestroyBuffer(allocator, bufferHandle, allocationHandle);
}

void DeviceAllocator::destroyImage(AllocatedImage image) const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    auto&& allocationHandle = static_cast<VmaAllocation>(image.getAllocation());
    auto&& imageHandle = static_cast<VkImage>(static_cast<vk::Image>(image));
    vmaDestroyImage(allocator, imageHandle, allocationHandle);
}

void DeviceAllocator::destroy() const noexcept
{
    auto&& allocator = static_cast<VmaAllocator>(handle);
    vmaDestroyAllocator(allocator);
}

vk::ResultValue<UniqueAllocator>
UniqueAllocator::makeUnique(vk::Instance instance, vk::Device device, vk::PhysicalDevice physicalDevice,
                            vk::detail::DispatchLoaderDynamic const& dispatcher) noexcept
{
    auto result = DeviceAllocator::create(instance, device, physicalDevice, dispatcher);
    return vk::ResultValue{result.result, UniqueAllocator{result.value}};
}

void UniqueAllocatedBufferDeleter::operator()(AllocatedBuffer const& allocatedBuffer) const noexcept
{
    allocator->destroyBuffer(allocatedBuffer);
}

void UniqueAllocatedImageDeleter::operator()(AllocatedImage const& allocatedImage) const noexcept
{
    allocator->destroyImage(allocatedImage);
}
} // namespace shuttle::resources
