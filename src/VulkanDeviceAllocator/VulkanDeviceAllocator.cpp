#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#define VK_NO_PROTOTYPES
#define VMA_IMPLEMENATION
#include "vk_mem_alloc.h"
#include "VulkanDeviceAllocator.hpp"

namespace vma {

	vk::ResultValue<Allocator> Allocator::create(
		vk::Instance instance, vk::Device device,
		vk::PhysicalDevice physicalDevice,
		vk::detail::DispatchLoaderDynamic const& dispatcher) {
		
		VmaAllocator resultAllocator;

		VmaVulkanFunctions vulkanFunctions{
			.vkGetInstanceProcAddr = dispatcher.vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = dispatcher.vkGetDeviceProcAddr,
			.vkGetPhysicalDeviceProperties = dispatcher.vkGetPhysicalDeviceProperties,
			.vkGetPhysicalDeviceMemoryProperties = dispatcher.vkGetPhysicalDeviceMemoryProperties,
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
			.vkDestroyImage = dispatcher.vkDestroyImage
		};

		VmaAllocatorCreateInfo allocatorCreateInfo{
			.flags = 0,
			.physicalDevice = physicalDevice,
			.device = device,
			.preferredLargeHeapBlockSize = 0,
			.pAllocationCallbacks = nullptr,
			.pDeviceMemoryCallbacks = nullptr,
			.pHeapSizeLimit = nullptr,
			.pVulkanFunctions = &vulkanFunctions,
			.instance = instance,
			.vulkanApiVersion = VK_API_VERSION_1_0,
			.pTypeExternalMemoryHandleTypes = nullptr
		};

		auto result = vmaCreateAllocator(
			&allocatorCreateInfo,
			&resultAllocator
		);
		return vk::ResultValue<Allocator>{static_cast<vk::Result>(result), Allocator{resultAllocator}};
	}

	vk::ResultValue<AllocatedBuffer> Allocator::createAndAllocateBuffer(
		vk::BufferCreateInfo const& bufferCreateInfo, 
		vma::MemoryUsage desiredMemoryUsage, 
		vma::AllocationCreateFlags allocationCreateFlags) const
	{
		auto allocationCreateFlagsInt{ static_cast<uint32_t>(allocationCreateFlags) };
		auto memoryUsageInt{ static_cast<VmaMemoryUsage>(desiredMemoryUsage) };

			VmaAllocationCreateInfo allocationCreateInfo{
			.flags = allocationCreateFlagsInt,
			.usage = memoryUsageInt,
			.requiredFlags = 0,
			.preferredFlags = 0,
			.memoryTypeBits = 0,
			.pool = nullptr,
			.pUserData = nullptr,
			.priority = 0.0f
		};

		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);

		VmaAllocation allocation;
		VkBuffer buffer;

		auto result = vmaCreateBuffer(
			allocator,
			bufferCreateInfo,
			&allocationCreateInfo,
			&buffer, &allocation, nullptr
		);

		return vk::ResultValue<AllocatedBuffer>{static_cast<vk::Result>(result), AllocatedBuffer{allocation, buffer}};
	}

	vk::ResultValue<AllocatedImage> Allocator::createAndAllocateImage(
		vk::ImageCreateInfo const& imageCreateInfo,
		vma::MemoryUsage desiredMemoryUsage,
		vma::AllocationCreateFlags allocationCreateFlags
	) const {
		auto allocationCreateFlagsInt{ static_cast<uint32_t>(allocationCreateFlags) };
		auto memoryUsageInt{ static_cast<VmaMemoryUsage>(desiredMemoryUsage) };
		VmaAllocationCreateInfo allocationCreateInfo{
			.flags = allocationCreateFlagsInt,
			.usage = memoryUsageInt,
			.requiredFlags = 0,
			.preferredFlags = 0,
			.memoryTypeBits = 0,
			.pool = nullptr,
			.pUserData = nullptr,
			.priority = 0.0f
		};
		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);
		VmaAllocation allocation;
		VkImage image;
		auto result = vmaCreateImage(
			allocator,
			imageCreateInfo,
			&allocationCreateInfo,
			&image, &allocation, nullptr
		);
		return vk::ResultValue<AllocatedImage>{static_cast<vk::Result>(result), AllocatedImage{ allocation, image }};
	}

	vk::ResultValue<UniqueAllocatedBuffer> Allocator::createAndAllocateBufferUnique(
		vk::BufferCreateInfo const& bufferCreateInfo,
		vma::MemoryUsage desiredMemoryUsage,
		vma::AllocationCreateFlags allocationCreateFlags
	) const
	{
		auto result = createAndAllocateBuffer(bufferCreateInfo, desiredMemoryUsage, allocationCreateFlags);
		if (!result.has_value()) {
			return vk::ResultValue<UniqueAllocatedBuffer>{result.result, UniqueAllocatedBuffer{}};
		}
		return vk::ResultValue<UniqueAllocatedBuffer>{result.result, UniqueAllocatedBuffer{ result.value, UniqueAllocatedBufferDeleter{*this} }};
	}

	vk::ResultValue<UniqueAllocatedImage> Allocator::createAndAllocateImageUnique(
		vk::ImageCreateInfo const& imageCreateInfo,
		vma::MemoryUsage desiredMemoryUsage,
		vma::AllocationCreateFlags allocationCreateFlags
	) const
	{
		auto result = createAndAllocateImage(imageCreateInfo, desiredMemoryUsage, allocationCreateFlags);
		if (!result.has_value()) {
			return vk::ResultValue<UniqueAllocatedImage>{result.result, UniqueAllocatedImage{}};
		}
		return vk::ResultValue<UniqueAllocatedImage>{result.result, UniqueAllocatedImage{ result.value, UniqueAllocatedImageDeleter{*this} }};
	}

	vk::Result Allocator::writeBufferFromHost(BufferWriteInfo const& bufferWriteInfo) const
	{
		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);
		auto&& allocationHandle = reinterpret_cast<VmaAllocation>(bufferWriteInfo.dstBuffer.getAllocation());
		void* mappedData = nullptr;
		VmaAllocationInfo allocationInfo;
		vmaGetAllocationInfo(allocator, allocationHandle, &allocationInfo);

		if (allocationInfo.pMappedData) {
			std::memcpy(static_cast<char*>(allocationInfo.pMappedData) + bufferWriteInfo.dstBufferOffset, bufferWriteInfo.srcData, bufferWriteInfo.dataSize);
			return vk::Result::eSuccess;
		}

		auto mapResult = vmaMapMemory(allocator, allocationHandle, &mappedData);
		std::memcpy(static_cast<char*>(mappedData) + bufferWriteInfo.dstBufferOffset, bufferWriteInfo.srcData, bufferWriteInfo.dataSize);
		vmaUnmapMemory(allocator, allocationHandle);
		return vk::Result::eSuccess;
	}

	vk::Result Allocator::readBufferToHost(BufferReadInfo const& bufferReadInfo) const
	{
		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);
		auto&& allocationHandle = reinterpret_cast<VmaAllocation>(bufferReadInfo.srcBuffer.getAllocation());
		void* mappedData;
		vmaMapMemory(allocator, allocationHandle, &mappedData);
		std::memcpy(bufferReadInfo.dstData, static_cast<char*>(mappedData) + bufferReadInfo.srcBufferOffset, bufferReadInfo.dataSize);
		vmaUnmapMemory(allocator, allocationHandle);
		return vk::Result::eSuccess;
	}

	void Allocator::destroyBuffer(AllocatedBuffer buffer) const
	{
		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);
		auto&& allocationHandle = reinterpret_cast<VmaAllocation>(buffer.getAllocation());
		auto&& bufferHandle = static_cast<VkBuffer>(static_cast<vk::Buffer>(buffer));
		vmaDestroyBuffer(allocator, bufferHandle, allocationHandle);
	}

	void Allocator::destroyImage(AllocatedImage image) const
	{
		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);
		auto&& allocationHandle = reinterpret_cast<VmaAllocation>(image.getAllocation());
		auto&& imageHandle = static_cast<VkImage>(static_cast<vk::Image>(image));
		vmaDestroyImage(allocator, imageHandle, allocationHandle);
	}

	void Allocator::destroy() const
	{
		auto&& allocator = reinterpret_cast<VmaAllocator>(handle);
		vmaDestroyAllocator(allocator);
	}

	vk::ResultValue<UniqueAllocator> UniqueAllocator::makeUnique(
		vk::Instance instance, vk::Device device,
		vk::PhysicalDevice physicalDevice,
		vk::detail::DispatchLoaderDynamic const& dispatcher) {
		auto result = Allocator::create(instance, device, physicalDevice, dispatcher);
		return vk::ResultValue<UniqueAllocator>{result.result, UniqueAllocator{result.value}};
	}


	void UniqueAllocatedBufferDeleter::operator()(AllocatedBuffer const& allocatedBuffer) const
	{
		allocator->destroyBuffer(allocatedBuffer);
	}

	void UniqueAllocatedImageDeleter::operator()(AllocatedImage const& allocatedImage) const
	{
		allocator->destroyImage(allocatedImage);
	}

}