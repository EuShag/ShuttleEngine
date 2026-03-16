#pragma once
#include <functional>
#include <map>
#include <vector>
#include "IncludeVulkan.hpp"
#include "../VulkanDebugger/VulkanDebugger.hpp"
#include "../VulkanStructureChain/VulkanStructureChain.hpp"

class VulkanInstanceBuilder {
public:
	VulkanInstanceBuilder(std::function<void(VulkanInstanceBuilder&)> const& builderCallback);

	VulkanInstanceBuilder& addLayers(std::vector<char const*> layers);
	VulkanInstanceBuilder& addExtensions(std::vector<char const*> extensions);
	VulkanInstanceBuilder& setDebugMessenger(VulkanDebugger debugger);
	VulkanInstanceBuilder& setupApplicationInfo(vk::ApplicationInfo const& applicationInfo);

	vk::ResultValue<vk::Instance> build(vk::detail::DispatchLoaderDynamic const& dynamicDispatch  = ::vk::detail::defaultDispatchLoaderDynamic);
	vk::ResultValue<vk::UniqueInstance> buildUnique(vk::detail::DispatchLoaderDynamic const& dynamicDispatch = ::vk::detail::defaultDispatchLoaderDynamic);
private:
	VulkanStructureChain structureChain;
	std::vector<char const*> enabledLayers{};
	std::vector<char const*> enabledExtensions{};
	vk::ApplicationInfo applicationInfo{
		.pApplicationName = "Vulkan Application",
		.applicationVersion = vk::makeVersion(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = vk::makeVersion(1, 0, 0),
		.apiVersion = vk::ApiVersion10
	};
};