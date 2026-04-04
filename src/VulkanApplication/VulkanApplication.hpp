#pragma once
#include "IncludeVulkan.hpp"

class VulkanApplication {
public:
	VulkanApplication();


private:
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;
	vk::UniqueDevice device;
};