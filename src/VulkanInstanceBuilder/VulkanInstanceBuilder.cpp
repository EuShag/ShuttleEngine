#include "VulkanInstanceBuilder.hpp"
#include "../VulkanHelperFunctions/VulkanHelperFunctions.hpp"

VulkanInstanceBuilder::VulkanInstanceBuilder(std::function<void(VulkanInstanceBuilder&)> const& builderCallback) {
	builderCallback(*this);
}

VulkanInstanceBuilder& VulkanInstanceBuilder::addLayers(std::vector<char const*> layers) {
	checkLayersSupport(layers);
	enabledLayers.insert(enabledLayers.end(), layers.begin(), layers.end());
	return *this;
}
VulkanInstanceBuilder& VulkanInstanceBuilder::addExtensions(std::vector<char const*> extensions) {
	checkExtensionSupport(extensions);
	enabledExtensions.insert(enabledExtensions.end(), extensions.begin(), extensions.end());
	return *this;
}
VulkanInstanceBuilder& VulkanInstanceBuilder::setDebugMessenger(VulkanDebugger debugger) {
	structureChain.add(debugger.getDebugMessengerCreateInfo());
	return *this;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::setupApplicationInfo(vk::ApplicationInfo const& applicationInfoIn) {
	this->applicationInfo = applicationInfoIn;
	return *this;
}

vk::ResultValue<vk::Instance> VulkanInstanceBuilder::build(vk::detail::DispatchLoaderDynamic const& dynamicDispatch)
{
	vk::InstanceCreateInfo instanceCreateInfo{
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size()),
		.ppEnabledLayerNames = enabledLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
		.ppEnabledExtensionNames = enabledExtensions.data(),
	};

	instanceCreateInfo.pNext = structureChain.getChainHead();

	return vk::createInstance(instanceCreateInfo, nullptr, dynamicDispatch);
}

vk::ResultValue<vk::UniqueInstance> VulkanInstanceBuilder::buildUnique(vk::detail::DispatchLoaderDynamic const& dynamicDispatch) {
	auto [result, instance] = build(dynamicDispatch);
	return vk::ResultValue{ result, 
		vk::UniqueInstance(instance, 
		                   vk::detail::ObjectDestroy<vk::detail::NoParent, vk::detail::DispatchLoaderDynamic>{nullptr, dynamicDispatch}) };
}

void setupApplicationInfo(vk::ApplicationInfo const& applicationInfo);
