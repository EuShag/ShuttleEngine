#pragma once
#include <vector>
#include "IncludeVulkan.hpp"

vk::ResultValue<vk::UniqueShaderModule> loadAndCreateShaderModule(vk::Device const& device, vk::PipelineStageFlagBits shaderStage, char const* filePath);
bool checkExtensionSupport(std::vector<char const*> const& requiredExtensions);
bool checkLayersSupport(std::vector<char const*> const& requiredLayers);
bool checkExtensionsSupport(vk::PhysicalDevice const& physicalDevice, std::vector<char const*> const& requiredExtensions);
uint32_t findMemoryTypeIndex(vk::PhysicalDevice const& physicalDevice, uint32_t memoryTypeBits, vk::MemoryPropertyFlags requiredProperties);