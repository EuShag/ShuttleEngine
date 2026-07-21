#pragma once
#include <string>
#include <vulkan/vulkan.hpp>

struct Image1D16bitData {
	uint32_t width = 0;
	uint32_t height = 0;
	uint16_t* data = nullptr;
};

class Image1D16bit {
public:
	explicit Image1D16bit(std::string const& filePath);

	[[nodiscard]] uint16_t getTexelValue(uint32_t x, uint32_t y) const;
	[[nodiscard]] float getTexelValueNormalized(uint32_t x, uint32_t y) const;

	[[nodiscard]] float sampleBilinear(float u, float v) const;

	~Image1D16bit();

private:
	Image1D16bitData imageData;
};
