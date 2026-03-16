#pragma once
#include "IncludeVulkan.hpp"

struct VulkanQueueRequirements {
	vk::QueueFlags requiredQueueFlags = {};
	bool presentationSupportRequired = false;
	bool dedicatedQueueRequired = false;
	bool dedicatedQueueFamilyIndexRequired = false;
};

struct VulkanDeviceRequirements {
	
};

class VulkanPhysicalDevicePicker {
public:
	vk::PhysicalDevice pick();
private:
	std::vector<VulkanQueueRequirements> queueRequirements;
};