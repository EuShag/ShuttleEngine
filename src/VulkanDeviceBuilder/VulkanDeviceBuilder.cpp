#include "VulkanDeviceBuilder.hpp"

VulkanDeviceBuilder::VulkanDeviceBuilder(std::function<void(VulkanDeviceBuilder&)> const& vulkanDeviceBuilderCallback) {
	vulkanDeviceBuilderCallback(*this);
}

VulkanDeviceBuilder& VulkanDeviceBuilder::addExtension(std::vector<char const*> addExtensionNames) {
	this->extensionNames.insert(this->extensionNames.end(), addExtensionNames.begin(), addExtensionNames.end());
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::setApiVersion(uint32_t inApiVersion) {
	apiVersion = inApiVersion;
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::enableSamplerAnisotropy() {
	if (pNextChain.contains<vk::PhysicalDeviceFeatures2>()) {
		vk::PhysicalDeviceFeatures2& features2 = pNextChain.get<vk::PhysicalDeviceFeatures2>();
		features2.features.samplerAnisotropy = vk::True;
		return *this;
	}
	vk::PhysicalDeviceFeatures2 features2;
	features2.features.samplerAnisotropy = vk::True;
	pNextChain.add(features2);
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::enableDynamicRendering() {
	if (apiVersion < vk::ApiVersion13 && !std::ranges::count(extensionNames, vk::KHRDynamicRenderingExtensionName)) {
		throw std::runtime_error("Dynamic rendering requires either Vulkan 1.3 or the VK_KHR_dynamic_rendering extension.");
	}

	if (apiVersion >= vk::ApiVersion13) {
		if (pNextChain.contains<vk::PhysicalDeviceVulkan13Features>()) {
			vk::PhysicalDeviceVulkan13Features& features13 = pNextChain.get<vk::PhysicalDeviceVulkan13Features>();
			features13.dynamicRendering = vk::True;
		}
		else{
			vk::PhysicalDeviceVulkan13Features features13;
			features13.dynamicRendering = vk::True;
			pNextChain.add(features13);
		}
	}
	else {
		if (pNextChain.contains<vk::PhysicalDeviceDynamicRenderingFeaturesKHR>()) {
			vk::PhysicalDeviceDynamicRenderingFeaturesKHR& dynamicRenderingFeatures = pNextChain.get<vk::PhysicalDeviceDynamicRenderingFeaturesKHR>();
			dynamicRenderingFeatures.dynamicRendering = vk::True;
		}
		else{
			vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures;
			dynamicRenderingFeatures.dynamicRendering = vk::True;
			pNextChain.add(dynamicRenderingFeatures);
		}
	}
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::enableDynamicRenderingLocalRead() {
	// This feature is available in Vulkan 1.4 or VK_KHR_dynamic_rendering_local_read extension

	if (apiVersion < vk::ApiVersion14 && !std::ranges::count(extensionNames, vk::KHRDynamicRenderingLocalReadExtensionName)) {
		throw std::runtime_error("Dynamic rendering local read requires either Vulkan 1.4 or the VK_KHR_dynamic_rendering_local_read extension.");
	}

	if (apiVersion >= vk::ApiVersion14) {
		if (pNextChain.contains<vk::PhysicalDeviceVulkan14Features>()) {
			vk::PhysicalDeviceVulkan14Features& features14 = pNextChain.get<vk::PhysicalDeviceVulkan14Features>();
			features14.dynamicRenderingLocalRead = vk::True;
		}
		else{
			vk::PhysicalDeviceVulkan14Features features14;
			features14.dynamicRenderingLocalRead = vk::True;
			pNextChain.add(features14);
		}
	}
	else {
		if (pNextChain.contains<vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR>()) {
			vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR& localReadFeatures = pNextChain.get<vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR>();
			localReadFeatures.dynamicRenderingLocalRead = vk::True;
		}
		else{
			vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR localReadFeatures;
			localReadFeatures.dynamicRenderingLocalRead = vk::True;
			pNextChain.add(localReadFeatures);
		}
	}
	return *this;	
}
/*
VulkanDeviceBuilder& VulkanDeviceBuilder::addQueue(VulkanQueueBuilder& queueBuilder) {
	queueCreateInfos.push_back(
		VulkanQueueBuilder(queueBuilder).build()
	);
	return *this;
}*/

vk::ResultValue<vk::Device> VulkanDeviceBuilder::build(
	vk::PhysicalDevice physicalDevice,
	vk::Optional<vk::AllocationCallbacks const> const& allocationCallbacks,
	vk::detail::DispatchLoaderDynamic const& dynamicDispatch) const
{
	return physicalDevice.createDevice(
		vk::DeviceCreateInfo{
			.pNext = pNextChain.getChainHead(),
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
			.ppEnabledExtensionNames = extensionNames.data(),
			.pEnabledFeatures = nullptr
		},
		allocationCallbacks,
		dynamicDispatch
	);
}

vk::ResultValue<vk::UniqueDevice> VulkanDeviceBuilder::buildUnique(
	vk::PhysicalDevice physicalDevice,
	vk::Optional<vk::AllocationCallbacks const> const& allocationCallbacks,
	vk::detail::DispatchLoaderDynamic const& dynamicDispatch) const
{
	return physicalDevice.createDeviceUnique(
		vk::DeviceCreateInfo{
			.pNext = pNextChain.getChainHead(),
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
			.ppEnabledExtensionNames = extensionNames.data(),
			.pEnabledFeatures = nullptr
		},
		allocationCallbacks,
		dynamicDispatch
	);
}