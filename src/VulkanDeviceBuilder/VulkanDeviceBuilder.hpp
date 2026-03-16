#pragma once
#include <functional>
#include <vector>
#include "IncludeVulkan.hpp"
#include "../VulkanStructureChain/VulkanStructureChain.hpp"

class VulkanQueueBuilder {
public:
	explicit VulkanQueueBuilder(std::function<void(VulkanQueueBuilder&)> const& vulkanQueueBuilderCallback);
	VulkanQueueBuilder& setQueueFamilyIndex(uint32_t queueFamilyIndex);
	VulkanQueueBuilder& setQueuesPriorities(std::vector<float> priorities);
	VulkanQueueBuilder& setQueuesGlobalPriority(vk::QueueGlobalPriority globalPriority);
	VulkanQueueBuilder& setQueuesFlags(vk::DeviceQueueCreateFlags flags);

	[[nodiscard]] vk::DeviceQueueCreateInfo build() const;
private:
	VulkanStructureChain pNextChain;
	vk::DeviceQueueCreateFlags flags{};
	vk::QueueGlobalPriority globalPriority{};
};

class VulkanDeviceBuilder {
public:
	explicit VulkanDeviceBuilder(std::function<void(VulkanDeviceBuilder&)> const& vulkanDeviceBuilderCallback);
	VulkanDeviceBuilder& addExtension(std::vector<char const*> addExtensionNames);
	VulkanDeviceBuilder& setApiVersion(uint32_t apiVersion);

	VulkanDeviceBuilder& enableSamplerAnisotropy();
	VulkanDeviceBuilder& enableDynamicRendering();
	VulkanDeviceBuilder& enableDynamicRenderingLocalRead();

	VulkanDeviceBuilder& addQueue(VulkanQueueBuilder& queueBuilder);

	[[nodiscard]] vk::ResultValue<vk::Device> build(
		vk::PhysicalDevice physicalDevice, 
		vk::Optional<vk::AllocationCallbacks const> const& allocationCallbacks = nullptr, 
		vk::detail::DispatchLoaderDynamic const& dynamicDispatch = vk::detail::defaultDispatchLoaderDynamic) const;
	[[nodiscard]] vk::ResultValue<vk::UniqueDevice> buildUnique(
		vk::PhysicalDevice physicalDevice,
		vk::Optional<vk::AllocationCallbacks const> const& allocationCallbacks = nullptr,
		vk::detail::DispatchLoaderDynamic const& dynamicDispatch = vk::detail::defaultDispatchLoaderDynamic) const;
private:
	VulkanStructureChain pNextChain;
	alignas(8) vk::DeviceCreateFlags deviceCreateFlags{};
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};
	std::vector<char const*> extensionNames{};
	uint32_t apiVersion{ vk::ApiVersion10 };
};