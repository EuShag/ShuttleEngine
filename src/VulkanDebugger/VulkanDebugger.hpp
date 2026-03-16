#pragma once
#include "IncludeVulkan.hpp"
#include <iostream>

class VulkanDebugger {
public:
	vk::DebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() const {
		return vk::DebugUtilsMessengerCreateInfoEXT{
			.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
			.pfnUserCallback = debugCallback,
			.pUserData = nullptr
		};
	}
private:
	// Debug messenger callback function
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void*) {
		if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
			std::cerr << "Validation layer: " << pCallbackData->pMessage << '\n';
		}
		return vk::False;
	}
};