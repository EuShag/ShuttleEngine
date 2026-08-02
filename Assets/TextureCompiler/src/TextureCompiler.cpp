#include <Assets/TextureCompiler/TextureCompiler.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <system_error>

#include <glm/glm.hpp>

#include <mio/mio.hpp>

#include <omp.h>

#include <vulkan/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define TINYDDSLOADER_IMPLEMENTATION

#include "rgbcx.h"
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <tinyddsloader.h>

#include "ispc_texcomp/ispc_texcomp.h"

namespace shuttle::assets::texture_compiler
{
namespace texture_format = shuttle::assets::formats::texture;

    namespace {
        std::vector<uint8_t> compressBC5(
        const uint8_t* rgbaPixels,
        int width,
        int height) {

            rgbcx::init();

            const int blocksX = (width + 3) / 4;
            const int blocksY = (height + 3) / 4;

            std::vector<uint8_t> output(
                static_cast<size_t>(blocksX) *
                static_cast<size_t>(blocksY) * 16);

            uint8_t blockPixels[4 * 4 * 4];

            for (int by = 0; by < blocksY; ++by)
            {
                for (int bx = 0; bx < blocksX; ++bx)
                {
                    // Собираем RGBA-блок 4x4
                    for (int y = 0; y < 4; ++y)
                    {
                        const int srcY = std::min(by * 4 + y, height - 1);

                        for (int x = 0; x < 4; ++x)
                        {
                            const int srcX = std::min(bx * 4 + x, width - 1);

                            const size_t srcOffset =
                                (static_cast<size_t>(srcY) * width + srcX) * 4;

                            const size_t dstOffset =
                                (y * 4 + x) * 4;

                            blockPixels[dstOffset + 0] =
                                rgbaPixels[srcOffset + 0];

                            blockPixels[dstOffset + 1] =
                                rgbaPixels[srcOffset + 1];

                            blockPixels[dstOffset + 2] =
                                rgbaPixels[srcOffset + 2];

                            blockPixels[dstOffset + 3] =
                                rgbaPixels[srcOffset + 3];
                        }
                    }

                    uint8_t* dstBlock =
                        output.data() +
                        ((by * blocksX + bx) * 16);

                    rgbcx::encode_bc5(
                        dstBlock,
                        blockPixels,
                        0, // R = X
                        1, // G = Y
                        4  // RGBA stride
                    );
                }
            }

            return output;
        }
    }

std::optional<CompiledTexture> TextureCompiler::compileFile(const std::filesystem::path& filePath,
                                                            const TextureCompileOptions& options)
{
    mio::mmap_source mapping;
    std::error_code error;

    mapping.map(filePath.string(), error);

    if (error || mapping.empty())
    {
        std::cerr << "[TextureCompiler] Failed to map texture file: " << filePath << std::endl;

        return std::nullopt;
    }

    const std::string extension = filePath.extension().string();

    return compileMemory(ImageData{.pixels = reinterpret_cast<const uint8_t*>(mapping.data()),

                                   .size = mapping.size()},
                         extension, options);
}

std::optional<CompiledTexture> TextureCompiler::compileMemory(const ImageData& imageData, const std::string& formatHint,
                                                              const TextureCompileOptions& options)
{
    if (!imageData.valid())
    {
        return std::nullopt;
    }

    const std::string extension = normalizeExtension(formatHint);

    if (extension == "dds")
    {
        return importDDS(imageData.pixels, imageData.size);
    }

    if (extension == "ktx" || extension == "ktx2")
    {
        std::cerr << "[TextureCompiler] KTX/KTX2 import is not implemented yet." << std::endl;

        return std::nullopt;
    }

    return importSTB(imageData.pixels, imageData.size, options);
}

std::optional<CompiledTexture> TextureCompiler::compileRGBA8(const ImageSizedData& imageData,
                                                             const TextureCompileOptions& options)
{
    if (!imageData.valid())
    {
        return std::nullopt;
    }

    CompiledTexture output{};

    const uint32_t mipCount = options.generateMips
            ? static_cast<uint32_t>(std::floor(std::log2(std::max(imageData.width, imageData.height))) + 1.0f) : 1u;

    std::vector currentMipPixels(imageData.pixels,
                                          imageData.pixels + static_cast<size_t>(imageData.width) *
                                                                 static_cast<size_t>(imageData.height) * 4u);

    int currentWidth = static_cast<int>(imageData.width);

    int currentHeight = static_cast<int>(imageData.height);

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        std::vector<uint8_t> compressedLevel;

        if (options.format == VK_FORMAT_BC5_UNORM_BLOCK && options.semantic == texture_format::TextureSemantic::Normal)
        {
            renormalizeNormalMap(currentMipPixels, currentWidth, currentHeight);
            compressedLevel = compressBC5(currentMipPixels.data(), currentWidth, currentHeight);
        }
        else {
            const CompressionType compressionType = toCompressionType(options.format);

            compressedLevel = compressBlocks(
                currentMipPixels.data(),
                currentWidth, currentHeight,
                4,
                compressionType);
        }



        const uint64_t mipOffset = output.data.size();

        const uint64_t mipSize = compressedLevel.size();

        texture_format::TextureMipMetadata mipMetadata{};
        mipMetadata.dataOffset = mipOffset;
        mipMetadata.dataSize = mipSize;
        mipMetadata.width = static_cast<uint32_t>(currentWidth);
        mipMetadata.height = static_cast<uint32_t>(currentHeight);

        output.mipMetadata.push_back(mipMetadata);

        output.data.insert(output.data.end(), compressedLevel.begin(), compressedLevel.end());

        if (mip + 1 < mipCount)
        {
            const int nextWidth = std::max(1, currentWidth / 2);

            const int nextHeight = std::max(1, currentHeight / 2);

            std::vector<uint8_t> nextMipPixels(static_cast<size_t>(nextWidth) * static_cast<size_t>(nextHeight) * 4u);

            stbir_resize_uint8_linear(currentMipPixels.data(), currentWidth, currentHeight, 0, nextMipPixels.data(),
                                      nextWidth, nextHeight, 0, STBIR_RGBA);

            currentMipPixels = std::move(nextMipPixels);

            currentWidth = nextWidth;
            currentHeight = nextHeight;
        }
    }

    output.metadata.width = imageData.width;

    output.metadata.height = imageData.height;

    output.metadata.depth = 1;

    output.metadata.mipCount = mipCount;

    output.metadata.layerCount = 1;

    output.metadata.format = options.format;

    output.metadata.imageType = texture_format::ImageType::Image2D;

    output.metadata.imageViewType = texture_format::ImageViewType::View2D;

    output.metadata.mipTableOffset = 0;

    return output;
}

std::optional<CompiledTexture> TextureCompiler::compileRGBA16F(std::span<const ImageSizedData> mipChain,
                                                               const TextureCompileOptions& options,
                                                               uint32_t layerCount,
                                                               texture_format::ImageViewType imageViewType)
{
    if (mipChain.empty() || layerCount == 0)
    {
        return std::nullopt;
    }

    if (options.format != VK_FORMAT_BC6H_UFLOAT_BLOCK)
    {
        std::cerr << "[TextureCompiler] RGBA16F supports only BC6H_UFLOAT." << std::endl;

        return std::nullopt;
    }

    CompiledTexture output{};
    output.mipMetadata.reserve(mipChain.size());

    for (uint32_t mipLevel = 0; mipLevel < mipChain.size(); ++mipLevel)
    {
        const ImageSizedData& mipImage = mipChain[mipLevel];

        if (!mipImage.valid())
        {
            return std::nullopt;
        }

        const uint64_t mipOffset = static_cast<uint64_t>(output.data.size());

        uint64_t mipSize = 0;

        const auto* mipPixels = reinterpret_cast<const uint16_t*>(mipImage.pixels);

        const size_t layerElementCount =
            static_cast<size_t>(mipImage.width) * static_cast<size_t>(mipImage.height) * 4u;

        for (uint32_t layer = 0; layer < layerCount; ++layer)
        {
            const uint16_t* layerPixels = mipPixels + layerElementCount * static_cast<size_t>(layer);

            std::vector<uint8_t> compressedLayer =
                compressBlocks(reinterpret_cast<const uint8_t*>(layerPixels), static_cast<int>(mipImage.width),
                               static_cast<int>(mipImage.height), 8, CompressionType::BC6H);

            mipSize += static_cast<uint64_t>(compressedLayer.size());

            output.data.insert(output.data.end(), compressedLayer.begin(), compressedLayer.end());
        }

        texture_format::TextureMipMetadata mipMetadata{};
        mipMetadata.dataOffset = mipOffset;
        mipMetadata.dataSize = mipSize;
        mipMetadata.width = mipImage.width;
        mipMetadata.height = mipImage.height;

        output.mipMetadata.push_back(mipMetadata);
    }

    const ImageSizedData& baseMip = mipChain.front();

    output.metadata.width = baseMip.width;

    output.metadata.height = baseMip.height;

    output.metadata.depth = 1;

    output.metadata.mipCount = static_cast<uint32_t>(mipChain.size());

    output.metadata.layerCount = layerCount;

    output.metadata.format = VK_FORMAT_BC6H_UFLOAT_BLOCK;

    output.metadata.imageType = texture_format::ImageType::Image2D;

    output.metadata.imageViewType = imageViewType;

    output.metadata.mipTableOffset = 0;

    return output;
}

std::optional<CompiledTexture> TextureCompiler::compileFromRGBA(const uint8_t* rgbaPixels, uint32_t width,
                                                                uint32_t height, const TextureCompileOptions& options)
{
    return compileRGBA8(ImageSizedData{.pixels = rgbaPixels, .width = width, .height = height}, options);
}

std::optional<CompiledTexture> TextureCompiler::packORM(const std::optional<std::filesystem::path>& occlusionPath,
                                                        const std::optional<std::filesystem::path>& roughnessPath,
                                                        const std::optional<std::filesystem::path>& metallicPath,
                                                        const TextureCompileOptions& options)
{
    mio::mmap_source occlusionMapping;
    mio::mmap_source roughnessMapping;
    mio::mmap_source metallicMapping;

    std::error_code error;

    if (occlusionPath && !occlusionPath->empty())
    {
        occlusionMapping.map(occlusionPath->string(), error);

        if (error)
        {
            return std::nullopt;
        }
    }

    error.clear();

    if (roughnessPath && !roughnessPath->empty())
    {
        roughnessMapping.map(roughnessPath->string(), error);

        if (error)
        {
            return std::nullopt;
        }
    }

    error.clear();

    if (metallicPath && !metallicPath->empty())
    {
        metallicMapping.map(metallicPath->string(), error);

        if (error)
        {
            return std::nullopt;
        }
    }

    return packORM(
        ImageData{.pixels =
                      occlusionMapping.empty() ? nullptr : reinterpret_cast<const uint8_t*>(occlusionMapping.data()),

                  .size = occlusionMapping.size()},
        ImageData{.pixels =
                      roughnessMapping.empty() ? nullptr : reinterpret_cast<const uint8_t*>(roughnessMapping.data()),

                  .size = roughnessMapping.size()},
        ImageData{.pixels =
                      metallicMapping.empty() ? nullptr : reinterpret_cast<const uint8_t*>(metallicMapping.data()),

                  .size = metallicMapping.size()},
        options);
}

std::optional<CompiledTexture> TextureCompiler::packORM(const ImageData& occlusionData, const ImageData& roughnessData,
                                                        const ImageData& metallicData,
                                                        const TextureCompileOptions& options)
{
    int aoWidth = 0;
    int aoHeight = 0;
    int aoChannels = 0;

    int roughWidth = 0;
    int roughHeight = 0;
    int roughChannels = 0;

    int metallicWidth = 0;
    int metallicHeight = 0;
    int metallicChannels = 0;

    uint8_t* aoPixels = nullptr;
    uint8_t* roughPixels = nullptr;
    uint8_t* metallicPixels = nullptr;

    if (occlusionData.valid())
    {
        aoPixels = stbi_load_from_memory(occlusionData.pixels, static_cast<int>(occlusionData.size), &aoWidth,
                                         &aoHeight, &aoChannels, 4);
    }

    if (roughnessData.valid())
    {
        roughPixels = stbi_load_from_memory(roughnessData.pixels, static_cast<int>(roughnessData.size), &roughWidth,
                                            &roughHeight, &roughChannels, 4);
    }

    if (metallicData.valid())
    {
        metallicPixels = stbi_load_from_memory(metallicData.pixels, static_cast<int>(metallicData.size), &metallicWidth,
                                               &metallicHeight, &metallicChannels, 4);
    }

    const int targetWidth = std::max(1, std::max(aoPixels ? aoWidth : 1, std::max(roughPixels ? roughWidth : 1,
                                                                                  metallicPixels ? metallicWidth : 1)));

    const int targetHeight =
        std::max(1, std::max(aoPixels ? aoHeight : 1,
                             std::max(roughPixels ? roughHeight : 1, metallicPixels ? metallicHeight : 1)));

    auto resizeOrFill = [](uint8_t* sourcePixels, int sourceWidth, int sourceHeight, uint8_t fallbackValue,
                           int targetWidth, int targetHeight) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u);

        if (!sourcePixels)
        {
            std::fill(result.begin(), result.end(), fallbackValue);

            return result;
        }

        if (sourceWidth == targetWidth && sourceHeight == targetHeight)
        {
            std::memcpy(result.data(), sourcePixels, result.size());

            return result;
        }

        stbir_resize_uint8_linear(sourcePixels, sourceWidth, sourceHeight, 0, result.data(), targetWidth, targetHeight,
                                  0, STBIR_RGBA);

        return result;
    };

    std::vector<uint8_t> aoBuffer = resizeOrFill(aoPixels, aoWidth, aoHeight, 255, targetWidth, targetHeight);

    std::vector<uint8_t> roughBuffer =
        resizeOrFill(roughPixels, roughWidth, roughHeight, 255, targetWidth, targetHeight);

    std::vector<uint8_t> metallicBuffer =
        resizeOrFill(metallicPixels, metallicWidth, metallicHeight, 0, targetWidth, targetHeight);

    if (aoPixels)
    {
        stbi_image_free(aoPixels);
    }

    if (roughPixels)
    {
        stbi_image_free(roughPixels);
    }

    if (metallicPixels)
    {
        stbi_image_free(metallicPixels);
    }

    std::vector<uint8_t> ormPixels(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u);

    const bool roughnessIsGloss = options.roughnessIsGloss;

#pragma omp parallel for default(none)                                                                                 \
    shared(targetWidth, targetHeight, aoBuffer, roughBuffer, metallicBuffer, ormPixels, roughnessIsGloss)
    for (int y = 0; y < targetHeight; ++y)
    {
        for (int x = 0; x < targetWidth; ++x)
        {
            const size_t pixelOffset =
                (static_cast<size_t>(y) * static_cast<size_t>(targetWidth) + static_cast<size_t>(x)) * 4u;

            const uint8_t ao = aoBuffer[pixelOffset];

            uint8_t roughness = roughBuffer[pixelOffset];

            const uint8_t metallic = metallicBuffer[pixelOffset];

            if (roughnessIsGloss)
            {
                roughness = static_cast<uint8_t>(255u - roughness);
            }

            ormPixels[pixelOffset + 0] = ao;
            ormPixels[pixelOffset + 1] = roughness;
            ormPixels[pixelOffset + 2] = metallic;
            ormPixels[pixelOffset + 3] = 255;
        }
    }

    TextureCompileOptions ormOptions = options;

    ormOptions.semantic = texture_format::TextureSemantic::ORM;

    ormOptions.format = VK_FORMAT_BC7_UNORM_BLOCK;

    return compileRGBA8(ImageSizedData{.pixels = ormPixels.data(),
                                       .width = static_cast<uint32_t>(targetWidth),
                                       .height = static_cast<uint32_t>(targetHeight)},
                        ormOptions);
}

std::optional<CompiledTexture> TextureCompiler::importSTB(const uint8_t* data, size_t size,
                                                          const TextureCompileOptions& options)
{
    stbi_set_flip_vertically_on_load_thread(options.flipY ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels = 0;

    uint8_t* rawPixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);

    if (!rawPixels)
    {
        return std::nullopt;
    }

    std::optional<CompiledTexture> result = compileRGBA8(ImageSizedData{.pixels = rawPixels,
                                                                        .width = static_cast<uint32_t>(width),
                                                                        .height = static_cast<uint32_t>(height)},
                                                         options);

    stbi_image_free(rawPixels);

    return result;
}

std::optional<CompiledTexture> TextureCompiler::importDDS(const uint8_t* data, size_t size)
{
    tinyddsloader::DDSFile dds;

    if (dds.Load(data, size) != tinyddsloader::Result::Success)
    {
        return std::nullopt;
    }

    VkFormat vkFormat = VK_FORMAT_UNDEFINED;

    switch (dds.GetFormat())
    {
    case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm: vkFormat = VK_FORMAT_BC1_RGB_UNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB: vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm: vkFormat = VK_FORMAT_BC2_UNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB: vkFormat = VK_FORMAT_BC2_SRGB_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm: vkFormat = VK_FORMAT_BC3_UNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB: vkFormat = VK_FORMAT_BC3_SRGB_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm: vkFormat = VK_FORMAT_BC4_UNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm: vkFormat = VK_FORMAT_BC4_SNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm: vkFormat = VK_FORMAT_BC5_UNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm: vkFormat = VK_FORMAT_BC5_SNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm: vkFormat = VK_FORMAT_BC7_UNORM_BLOCK; break;

    case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB: vkFormat = VK_FORMAT_BC7_SRGB_BLOCK; break;

    default: return std::nullopt;
    }

    const uint32_t mipCount = dds.GetMipCount();

    const uint32_t layerCount = dds.GetArraySize();

    if (mipCount == 0 || layerCount == 0)
    {
        return std::nullopt;
    }

    CompiledTexture output{};

    output.mipMetadata.reserve(mipCount);

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        const uint64_t mipOffset = static_cast<uint64_t>(output.data.size());

        uint64_t mipSize = 0;

        const uint32_t mipWidth = std::max(1u, dds.GetWidth() >> mip);

        const uint32_t mipHeight = std::max(1u, dds.GetHeight() >> mip);

        for (uint32_t layer = 0; layer < layerCount; ++layer)
        {
            const auto* imageData = dds.GetImageData(mip, layer);

            if (!imageData || !imageData->m_mem)
            {
                return std::nullopt;
            }

            const auto* bytes = static_cast<const uint8_t*>(imageData->m_mem);

            output.data.insert(output.data.end(), bytes, bytes + imageData->m_memSlicePitch);

            mipSize += imageData->m_memSlicePitch;
        }

        texture_format::TextureMipMetadata mipMetadata{};
        mipMetadata.dataOffset = mipOffset;
        mipMetadata.dataSize = mipSize;
        mipMetadata.width = mipWidth;
        mipMetadata.height = mipHeight;

        output.mipMetadata.push_back(mipMetadata);
    }

    output.metadata.width = dds.GetWidth();

    output.metadata.height = dds.GetHeight();

    output.metadata.depth = 1;

    output.metadata.mipCount = mipCount;

    output.metadata.layerCount = layerCount;

    output.metadata.format = vkFormat;

    output.metadata.imageType = texture_format::ImageType::Image2D;

    if (dds.IsCubemap())
    {
        output.metadata.imageViewType = texture_format::ImageViewType::ViewCube;
    }
    else if (layerCount > 1)
    {
        output.metadata.imageViewType = texture_format::ImageViewType::View2DArray;
    }
    else
    {
        output.metadata.imageViewType = texture_format::ImageViewType::View2D;
    }

    output.metadata.mipTableOffset = 0;

    return output;
}

std::vector<uint8_t> TextureCompiler::compressBlocks(const uint8_t* pixels, int width, int height, int bytesPerPixel,
                                                     CompressionType compressionType)
{
    const int blocksX = (width + 3) / 4;

    const int blocksY = (height + 3) / 4;

    constexpr uint32_t bytesPerBlock = 16;

    std::vector<uint8_t> output(static_cast<size_t>(blocksX) * static_cast<size_t>(blocksY) * bytesPerBlock);

    const size_t rowStride = static_cast<size_t>(width) * static_cast<size_t>(bytesPerPixel);

#pragma omp parallel for default(none)                                                                                 \
    shared(blocksY, blocksX, width, height, bytesPerPixel, pixels, output, rowStride, compressionType)
    for (int blockY = 0; blockY < blocksY; ++blockY)
    {
        const int sourceY = blockY * 4;

        const int blockHeight = std::min(4, height - sourceY);

        const uint8_t* sourcePtr = pixels + static_cast<size_t>(sourceY) * rowStride;

        rgba_surface surface{};
        surface.ptr = const_cast<uint8_t*>(sourcePtr);

        surface.width = width;
        surface.height = blockHeight;
        surface.stride = static_cast<int>(rowStride);

        uint8_t* destination =
            output.data() + static_cast<size_t>(blockY) * static_cast<size_t>(blocksX) * bytesPerBlock;

        switch (compressionType)
        {

        case CompressionType::BC6H:
        {
            bc6h_enc_settings settings{};
            GetProfile_bc6h_basic(&settings);

            CompressBlocksBC6H(&surface, destination, &settings);
            break;
        }

        case CompressionType::BC7:
        {
            bc7_enc_settings settings{};
            GetProfile_basic(&settings);

            CompressBlocksBC7(&surface, destination, &settings);
            break;
        }
        }
    }

    return output;
}

void TextureCompiler::renormalizeNormalMap(std::vector<uint8_t>& rgbaPixels, int width, int height)
{
    const auto totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);

    auto* pixels = reinterpret_cast<glm::u8vec4*>(rgbaPixels.data());

#pragma omp parallel for default(none) shared(totalPixels, pixels)
    for (size_t i = 0; i < totalPixels; ++i)
    {
        glm::vec3 normal(pixels[i].x, pixels[i].y, pixels[i].z);

        normal = normal / 255.0f * 2.0f - 1.0f;

        const float lengthSq = glm::dot(normal, normal);

        if (lengthSq > 0.0001f)
        {
            normal = glm::inversesqrt(lengthSq) * normal;
        }
        else
        {
            normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        pixels[i].x = static_cast<uint8_t>((normal.x + 1.0f) * 0.5f * 255.0f);

        pixels[i].y = static_cast<uint8_t>((normal.y + 1.0f) * 0.5f * 255.0f);

        pixels[i].z = static_cast<uint8_t>((normal.z + 1.0f) * 0.5f * 255.0f);
    }
}

CompressionType TextureCompiler::toCompressionType(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_BC5_UNORM_BLOCK: return CompressionType::BC5;

    case VK_FORMAT_BC6H_UFLOAT_BLOCK: return CompressionType::BC6H;

    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    default: return CompressionType::BC7;
    }
}

std::string TextureCompiler::normalizeExtension(std::string extension)
{
    if (!extension.empty() && extension.front() == '.')
    {
        extension.erase(extension.begin());
    }

    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return extension;
}


} // namespace shuttle::assets::texture_compiler