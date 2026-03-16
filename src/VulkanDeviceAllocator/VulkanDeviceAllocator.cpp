#include "VulkanDeviceAllocator.hpp"

vk::ResultValue<vk::Buffer> VulkanDeviceAllocator::createAndBindToMemoryBuffer(vk::BufferCreateInfo const& bufferCreateInfo) {
	auto [result, buffer] = device.createBuffer(bufferCreateInfo);
	if (result != vk::Result::eSuccess) {
		return { result, vk::Buffer() };
	}
	auto const memoryRequirements = device.getBufferMemoryRequirements(buffer);
	auto [allocResult, uniqueDeviceMemory] = device.allocateMemoryUnique(vk::MemoryAllocateInfo{
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = 0, // Placeholder, should be determined based on requirements and properties
		});
	if (allocResult != vk::Result::eSuccess) {
		return { allocResult, vk::Buffer() };
	}
	if (auto bindMemoryResult = device.bindBufferMemory(buffer, *uniqueDeviceMemory, 0); bindMemoryResult != vk::Result::eSuccess) {
		return { bindMemoryResult, vk::Buffer() };
	}
	deviceMemories.emplace_back(std::move(uniqueDeviceMemory));
	return {vk::Result::eSuccess, buffer};
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
