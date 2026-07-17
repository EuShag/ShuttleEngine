#pragma once
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include "IncludeVulkan.hpp"
#include "../HostRenderData/HostRenderData.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"

shuttle_engine::HostMaterialData loadFromFiles(std::string const& albedoPath, std::string const& normalPath,
                                               std::string const& roughnessPath, std::string const& occlusionPath, std::string const& metallicPath, std::string const& emissionPath);



shuttle_engine::HostImageData loadImageFromFile(std::string filePath, vk::Format format);
shuttle_engine::HostImageData loadImageFromMemory(std::vector<unsigned char> const& imageData, vk::Format format);

std::optional<shuttle_engine::HostImageData> uniteSeparatedTexturesArm(
	std::optional<shuttle_engine::HostImageData> const& ambientTexture,
	std::optional<shuttle_engine::HostImageData> const& roughnessTexture,
	std::optional<shuttle_engine::HostImageData> const& metallicTexture
);

shuttle_engine::HostMaterialData loadFromFiles(std::string const& albedoPath, std::string const& normalPath, std::string const& ormPath, std::string const& emissionPath);

namespace shuttle_engine {

	enum class MipFilter {
		Box,      // Обычное среднее (для Albedo/ORM)
		NormalMap // С нормализацией (для Normal Maps)
	};


	inline void generateMipMaps(HostImageData const & imageData, MipFilter const filter) {
		// Временный буфер для текущего уровня
	    std::vector<uint8_t> currentLevelData = imageData.data;
	    uint32_t currentW = imageData.width;
	    uint32_t currentH = imageData.height;

	    while (currentW > 1 || currentH > 1) {
		    constexpr uint32_t channels = 4;
		    uint32_t const nextW = std::max(1u, currentW / 2);
	        uint32_t const nextH = std::max(1u, currentH / 2);
	        std::vector<uint8_t> nextLevelData(nextW * nextH * channels);

	        for (uint32_t y = 0; y < nextH; ++y) {
	            for (uint32_t x = 0; x < nextW; ++x) {
	                // Берем 4 пикселя (2x2) из предыдущего уровня
	                const uint8_t* p[4];
	                p[0] = &currentLevelData[((y * 2 + 0) * currentW + (x * 2 + 0)) * channels];
	                p[1] = &currentLevelData[((y * 2 + 0) * currentW + (x * 2 + 1)) * channels];
	                p[2] = &currentLevelData[((y * 2 + 1) * currentW + (x * 2 + 0)) * channels];
	                p[3] = &currentLevelData[((y * 2 + 1) * currentW + (x * 2 + 1)) * channels];

	                uint8_t* dst = &nextLevelData[(y * nextW + x) * channels];

	                if (filter == MipFilter::Box) {
	                    for (int c = 0; c < 4; ++c)
	                        dst[c] = (p[0][c] + p[1][c] + p[2][c] + p[3][c]) / 4;
	                }
	                else if (filter == MipFilter::NormalMap) {
	                    // Декодируем, суммируем векторы, нормализуем, кодируем
	                    glm::vec3 sum(0.0f);
	                    for (auto & i : p) {
	                        glm::vec3 const n = glm::vec3(i[0], i[1], i[2]) / 255.0f * 2.0f - 1.0f;
	                        sum += n;
	                    }
	                    glm::vec3 avg = glm::normalize(sum);
	                    glm::vec3 const res = (avg * 0.5f + 0.5f) * 255.0f;

	                    dst[0] = static_cast<uint8_t>(res.x); dst[1] = static_cast<uint8_t>(res.y); dst[2] = static_cast<uint8_t>(res.z);
	                    dst[3] = 255; // Alpha
	                }
	            }
	        }

	        // Здесь ты можешь сохранять nextLevelData в свой список уровней
	        currentLevelData = std::move(nextLevelData);
	        currentW = nextW;
	        currentH = nextH;
	    }
	}
}

struct ImageData {
	uint32_t width = 0;
	uint32_t height = 0;
	unsigned char* data = nullptr;
};

struct Image1D16bitData {
	uint32_t width = 0;
	uint32_t height = 0;
	uint16_t* data = nullptr;
};

struct CubeMapImageData {
	uint32_t sideWidth = 0;
	void* rightData = nullptr;
	void* leftData = nullptr;
	void* topData = nullptr;
	void* bottomData = nullptr;
	void* backData = nullptr;
	void* frontData = nullptr;
};

struct CubeMapImageFiles {
	std::string rightFilePath;
	std::string leftFilePath;
	std::string topFilePath;
	std::string bottomFilePath;
	std::string frontFilePath;
	std::string backFilePath;
};

struct StagingBufferData {
	vk::Buffer buffer;
	vk::DeviceMemory memory;
	vk::DeviceSize offset;
};

class ImageSampler {

};

class Image1D16bit {
public:

	Image1D16bit(std::string const& filePath);

	[[nodiscard]] uint16_t getTexelValue(uint32_t x, uint32_t y) const;
	[[nodiscard]] float getTexelValueNormalized(uint32_t x, uint32_t y) const;

	[[nodiscard]] float sampleBilinear(float u, float v) const;

	~Image1D16bit();

private:
	Image1D16bitData imageData;
};

class Image {
public:
	explicit Image(std::string const& filePath);

	[[nodiscard]] ImageData getData() const { return imageData; }
	[[nodiscard]] size_t getTotalSize() const { return imageData.width * imageData.height * 4; }

	~Image();

private:
	ImageData imageData{.width = 0, .height = 0, .data = nullptr};
};

class CubeMapImage {
public:
	CubeMapImage() = delete;
	explicit CubeMapImage(CubeMapImageFiles const& cubeMapImageFiles);
	[[nodiscard]] CubeMapImageData getData() const { return imageData; }
	[[nodiscard]] size_t getTotalDataSize() const { return static_cast<size_t>(imageData.sideWidth * imageData.sideWidth * 4) * 6; }

private:
	CubeMapImageData imageData{
		.sideWidth = 0,
		.rightData = nullptr, .leftData = nullptr, 
		.topData = nullptr, .bottomData = nullptr, 
		.backData = nullptr, .frontData = nullptr
	};
};