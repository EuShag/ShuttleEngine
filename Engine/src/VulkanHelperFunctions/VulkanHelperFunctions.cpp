#include "VulkanHelperFunctions.hpp"

#include <chrono>
#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <filesystem>
#include <iostream>
#include <stb_image_write.h>

namespace {

	std::filesystem::path makeScreenshotFileName()
	{
		namespace fs = std::filesystem;

		fs::create_directories("../resources/screenshots");

		auto now =
			std::chrono::system_clock::now();

		auto time =
			std::chrono::system_clock::to_time_t(now);

		std::tm localTime{};

		#ifdef _WIN32
		localtime_s(&localTime, &time);
		#else
		localtime_r(&time, &localTime);
		#endif

		std::stringstream ss;

		ss
			<< "../resources/screenshots"
			<< std::put_time(
				   &localTime,
				   "%Y-%m-%d_%H-%M-%S"
			   )
			<< ".png";

		return ss.str();
	}

}

vk::ResultValue<vk::UniqueShaderModule> loadAndCreateShaderModuleUnique(vk::Device const& device, char const* filePath) {
	std::fstream shaderModuleFile(filePath, std::ios::binary | std::ios::ate | std::ios::in);
	if (!shaderModuleFile.is_open()) {
		throw std::runtime_error("Failed to open fragment shader file");
	}
	auto const shaderModuleFileSize = shaderModuleFile.tellg();
	std::vector<char> fragmentShaderCode(shaderModuleFileSize);
	shaderModuleFile.seekg(0);
	shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

	return device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
		.codeSize = fragmentShaderCode.size(),
		.pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())
		});
}

vk::ResultValue<vk::ShaderModule> loadAndCreateShaderModule(vk::Device const& device, char const* filePath) {
	std::fstream shaderModuleFile(filePath, std::ios::binary | std::ios::ate | std::ios::in);
	if (!shaderModuleFile.is_open()) {
		throw std::runtime_error("Failed to open fragment shader file");
	}
	auto const shaderModuleFileSize = shaderModuleFile.tellg();
	std::vector<char> fragmentShaderCode(shaderModuleFileSize);
	shaderModuleFile.seekg(0);
	shaderModuleFile.read(fragmentShaderCode.data(), shaderModuleFileSize);

	return device.createShaderModule(vk::ShaderModuleCreateInfo{
		.codeSize = fragmentShaderCode.size(),
		.pCode = reinterpret_cast<uint32_t const*>(fragmentShaderCode.data())
		});
}

bool checkLayersSupport(std::vector<char const*> const& requiredLayers) {
	auto [result, availableLayers] = vk::enumerateInstanceLayerProperties();
	if (result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to enumerate instance layer properties");
	}
	for (char const* requiredLayer : requiredLayers) {
		bool found = false;
		for (auto const& [layerName, _1, _2, _3] : availableLayers) {
			if (strcmp(requiredLayer, layerName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

bool checkExtensionSupport(std::vector<char const*> const& requiredExtensions) {
	auto [result, availableExtensions] = vk::enumerateInstanceExtensionProperties();
	if (result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to enumerate instance extension properties");
	}

	for (char const* requiredExtension : requiredExtensions) {
		bool found = false;
		for (auto const& [extensionName, _] : availableExtensions) {
			if (strcmp(requiredExtension, extensionName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

bool checkExtensionsSupport(vk::PhysicalDevice const& physicalDevice, std::vector<char const*> const& requiredExtensions) {
	auto [result, availableExtensions] = physicalDevice.enumerateDeviceExtensionProperties();
	if (result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to enumerate device extension properties");
	}
	for (char const* requiredExtension : requiredExtensions) {
		bool found = false;
		for (auto const& [extensionName, _] : availableExtensions) {
			if (strcmp(requiredExtension, extensionName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

void strideCopy(void *dst, void const *src, size_t elementSize, size_t elementCount, size_t dstStride, size_t srcStride)
{
	char* dstChar = static_cast<char*>(dst);
	char const* srcChar = static_cast<char const*>(src);
	for (size_t i = 0; i < elementCount; ++i) {
		std::memcpy(dstChar, srcChar, elementSize);
		dstChar += dstStride;
		srcChar += srcStride;
	}
}

void safeScreenshot(void* screenshotBuffer, uint32_t width, uint32_t height) {
	auto filepath = makeScreenshotFileName();
	// Здесь можно добавить код для сохранения скриншота в файл с именем filename
	// Создадим файл по новому пути

	auto _width = static_cast<int32_t>(width);
	auto _height = static_cast<int32_t>(height);

	std::cout << "Safe Screenshot (" << _width << "x" << _height << ")" << "To" << filepath << " From " << screenshotBuffer << std::endl;


	for (size_t i = 0; i < width * height; ++i)
	{
		std::swap(
			static_cast<uint8_t*>(screenshotBuffer)[4 * i + 0],
			static_cast<uint8_t*>(screenshotBuffer)[4 * i + 2]
		);
	}


	auto result = stbi_write_png(
		filepath.string().c_str(),
		_width, _height, 4,
		screenshotBuffer,
		_width * 4
	);
	if (result == 0) {
		std::cerr << "Failed to save screenshot to " << filepath << std::endl;
	}
}
