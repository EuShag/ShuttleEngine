#include "VulkanDeviceAllocator.hpp"
// implementation of VulkanDeviceAllocator methods
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

VulkanDeviceAllocator::VulkanDeviceAllocator(vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool commandPool, vk::Queue transferQueue, vk::detail::DispatchLoaderDynamic const& dispatcher) {
	
	auto vulkanFunctions = new VmaVulkanFunctions {
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
	};

	allocator = new VmaAllocatorCreateInfo {
		.flags = 0,
		.physicalDevice = physicalDevice,
		.device = device,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = vulkanFunctions,
	};

	delete vulkanFunctions;
}

vk::ResultValue<vk::Buffer> VulkanDeviceAllocator::createAndBindToMemoryBuffer(vk::BufferCreateInfo const& bufferCreateInfo) {
	VmaAllocatorCreateInfo allocatorCreateInfo {
		.flags = 0,
		.physicalDevice = physicalDevice,
		.device = device,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = nullptr,
	};
}

vk::Optional<uint32_t> VulkanDeviceAllocator::findMemoryTypeIndex(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
	auto const memoryProperties = physicalDevice.getMemoryProperties();
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if (typeFilter & 1 << i && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return nullptr;
}
