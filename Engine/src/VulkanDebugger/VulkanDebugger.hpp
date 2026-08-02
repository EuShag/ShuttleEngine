#pragma once
#include <iostream>
#include "IncludeVulkan.hpp"

class VulkanDebugger
{
  public:
    vk::DebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfoHpp() const
    {
        return vk::DebugUtilsMessengerCreateInfoEXT{.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                                                    .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                                                    .pfnUserCallback = debugCallbackHpp,
                                                    .pUserData = nullptr};
    }

    VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() const
    {
        return VkDebugUtilsMessengerCreateInfoEXT{.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                  .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                                                  .pfnUserCallback = debugCallback,
                                                  .pUserData = nullptr};
    }

  private:
    // Debug messenger callback function
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL
    debugCallbackHpp(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT,
                     const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
    {
        if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        {
            std::cerr << "\tValidation layer: " << pCallbackData->pMessage << '\n';
        }
        return vk::False;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void*)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            std::cerr << "\tValidation layer: " << pCallbackData->pMessage << '\n';
        }
        return VK_FALSE;
    }
};