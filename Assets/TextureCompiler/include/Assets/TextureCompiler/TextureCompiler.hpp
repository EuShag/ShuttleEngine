#pragma once

#include <Assets/TextureCompiler/CompiledTexture.hpp>
#include <Assets/TextureCompiler/ImageData.hpp>
#include <Assets/TextureCompiler/TextureCompileOptions.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace shuttle::assets::texture_compiler
{
class TextureCompiler
{
  public:
    [[nodiscard]]
    static std::optional<CompiledTexture> compileFile(const std::filesystem::path& filePath,
                                                      const TextureCompileOptions& options);

    [[nodiscard]]
    static std::optional<CompiledTexture> compileMemory(const ImageData& imageData, const std::string& formatHint,
                                                        const TextureCompileOptions& options);

    [[nodiscard]]
    static std::optional<CompiledTexture> compileRGBA8(const ImageSizedData& imageData,
                                                       const TextureCompileOptions& options);

    [[nodiscard]]
    static std::optional<CompiledTexture>
    compileRGBA16F(std::span<const ImageSizedData> mipChain, const TextureCompileOptions& options,
                   uint32_t layerCount = 1,
                   formats::texture::ImageViewType imageViewType = formats::texture::ImageViewType::View2D);

    [[nodiscard]]
    static std::optional<CompiledTexture> compileFromRGBA(const uint8_t* rgbaPixels, uint32_t width, uint32_t height,
                                                          const TextureCompileOptions& options);

    [[nodiscard]]
    static std::optional<CompiledTexture> packORM(const std::optional<std::filesystem::path>& occlusionPath,
                                                  const std::optional<std::filesystem::path>& roughnessPath,
                                                  const std::optional<std::filesystem::path>& metallicPath,
                                                  const TextureCompileOptions& options);

    [[nodiscard]]
    static std::optional<CompiledTexture> packORM(const ImageData& occlusionData, const ImageData& roughnessData,
                                                  const ImageData& metallicData, const TextureCompileOptions& options);

  private:
    [[nodiscard]]
    static std::optional<CompiledTexture> importSTB(const uint8_t* data, size_t size,
                                                    const TextureCompileOptions& options);

    [[nodiscard]]
    static std::optional<CompiledTexture> importDDS(const uint8_t* data, size_t size);

    [[nodiscard]]
    static std::vector<uint8_t> compressBlocks(const uint8_t* pixels, int width, int height, int bytesPerPixel,
                                               CompressionType compressionType);

    static void renormalizeNormalMap(std::vector<uint8_t>& rgbaPixels, int width, int height);

    [[nodiscard]]
    static CompressionType toCompressionType(VkFormat format);

    [[nodiscard]]
    static std::string normalizeExtension(std::string extension);

    static std::vector<uint8_t> compressBlocksBC5FromRG8(const uint8_t *rgPixels, int width, int height);
};
} // namespace shuttle::assets::texture_compiler