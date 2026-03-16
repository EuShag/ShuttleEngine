#pragma once
#include <map>

#include "IncludeVulkan.hpp"
#include <vector>

class ImageMemoryResource {
public:

	vk::Image getImage() const { return image; }

private:
	vk::Image image;

	vk::DeviceMemory deviceMemory;
	vk::DeviceSize offset;
};

class VulkanDeviceAllocator {

	struct Binding {
		vk::DeviceMemory deviceMemory;
		uint64_t offset;
		uint64_t size;
	};

	struct Allocation {


	private:
		vk::DeviceMemory deviceMemory = VK_NULL_HANDLE;
		uint64_t offset = 0;
		uint64_t size = 0;
	};
public:
	VulkanDeviceAllocator(
		vk::Device device,
		vk::PhysicalDevice physicalDevice,
		vk::CommandPool commandPool,
		vk::Queue transferQueue
	) : device(device), physicalDevice(physicalDevice), commandPool(commandPool), graphicsQueue(transferQueue) {}

	[[nodiscard]] vk::ResultValue<vk::Buffer> createAndBindToMemoryBuffer(vk::BufferCreateInfo const& bufferCreateInfo);
	[[nodiscard]] vk::ResultValue<vk::Image> createAndBindToMemoryImage(vk::ImageCreateInfo const& imageCreateInfo);

	vk::ResultValue<void> writeBufferData(vk::DeviceMemory deviceMemory, vk::Buffer buffer, uint64_t size, uint64_t offset = 0);

private:
	std::vector<vk::UniqueDeviceMemory> deviceMemories;
	vk::Device device;
	vk::PhysicalDevice physicalDevice;
	vk::CommandPool commandPool;
	vk::Queue graphicsQueue;

	vk::Optional<uint32_t> findMemoryTypeIndex(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;
};