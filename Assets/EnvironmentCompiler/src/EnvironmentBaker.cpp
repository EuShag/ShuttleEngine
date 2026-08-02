#include "EnvironmentBaker.hpp"

#include <Assets/Formats/Environment.hpp>
#include <Assets/Formats/Texture.hpp>

#include <Assets/TextureCompiler/ImageData.hpp>
#include <Assets/TextureCompiler/TextureCompileOptions.hpp>
#include <Assets/TextureCompiler/TextureCompiler.hpp>

#include <cmft/cubemapfilter.h>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace shuttle::assets::environment_compiler
{
namespace
{
namespace texture_format = shuttle::assets::formats::texture;

namespace environment_format = shuttle::assets::formats::environment;

struct CmftImageGuard
{
    cmft::Image* image = nullptr;
    cmft::AllocatorI* allocator = nullptr;

    CmftImageGuard() = default;

    CmftImageGuard(cmft::Image* image, cmft::AllocatorI* allocator) : image(image), allocator(allocator) {}

    CmftImageGuard(const CmftImageGuard&) = delete;

    CmftImageGuard& operator=(const CmftImageGuard&) = delete;

    CmftImageGuard(CmftImageGuard&& other) noexcept : image(other.image), allocator(other.allocator)
    {
        other.image = nullptr;
        other.allocator = nullptr;
    }

    CmftImageGuard& operator=(CmftImageGuard&& other) noexcept
    {
        if (this != &other)
        {
            unload();

            image = other.image;
            allocator = other.allocator;

            other.image = nullptr;
            other.allocator = nullptr;
        }

        return *this;
    }

    ~CmftImageGuard() { unload(); }

    void unload()
    {
        if (image && image->m_data)
        {
            cmft::imageUnload(*image, allocator);
        }
    }
};

[[nodiscard]]
texture_compiler::TextureCompileOptions makeEnvironmentTextureOptions()
{
    texture_compiler::TextureCompileOptions options{};

    options.semantic = texture_format::TextureSemantic::IBL;

    options.format = VK_FORMAT_BC6H_UFLOAT_BLOCK;

    options.generateMips = false;

    options.flipY = false;

    return options;
}

[[nodiscard]]
uint8_t makeCmftThreadCount()
{
    const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());

    return static_cast<uint8_t>(std::min(hardwareThreads, 255u));
}

[[nodiscard]]
std::optional<texture_compiler::CompiledTexture> convertCmftImageToBc6h(const cmft::Image& image)
{
    if (!image.m_data)
    {
        std::cerr << "[EnvironmentBaker] CMFT image has no data." << std::endl;

        return std::nullopt;
    }

    if (image.m_width == 0 || image.m_height == 0)
    {
        std::cerr << "[EnvironmentBaker] CMFT image has invalid size." << std::endl;

        return std::nullopt;
    }

    const uint32_t faceCount = std::max(1u, static_cast<uint32_t>(image.m_numFaces));

    const uint32_t mipCount = std::max(1u, static_cast<uint32_t>(image.m_numMips));

    if (faceCount > CUBE_FACE_NUM)
    {
        std::cerr << "[EnvironmentBaker] Unsupported face count: " << faceCount << std::endl;

        return std::nullopt;
    }

    if (mipCount > MAX_MIP_NUM)
    {
        std::cerr << "[EnvironmentBaker] Unsupported mip count: " << mipCount << std::endl;

        return std::nullopt;
    }

    const auto& imageInfo = cmft::getImageDataInfo(image.m_format);

    const uint32_t bytesPerPixel = imageInfo.m_bytesPerPixel;

    if (bytesPerPixel == 0)
    {
        std::cerr << "[EnvironmentBaker] Unsupported CMFT texture format." << std::endl;

        return std::nullopt;
    }

    uint32_t mipOffsets[CUBE_FACE_NUM][MAX_MIP_NUM]{};

    cmft::imageGetMipOffsets(mipOffsets, image);

    size_t totalTexelCount = 0;

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        const uint32_t mipWidth = std::max(1u, image.m_width >> mip);

        const uint32_t mipHeight = std::max(1u, image.m_height >> mip);

        totalTexelCount +=
            static_cast<size_t>(mipWidth) * static_cast<size_t>(mipHeight) * static_cast<size_t>(faceCount);
    }

    std::vector<uint16_t> rgba16fData;
    rgba16fData.resize(totalTexelCount * 4u);

    const auto* sourceBytes = static_cast<const uint8_t*>(image.m_data);

    size_t destinationTexelOffset = 0;

    std::vector<texture_compiler::ImageSizedData> mipChain;
    mipChain.reserve(mipCount);

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        const uint32_t mipWidth = std::max(1u, image.m_width >> mip);

        const uint32_t mipHeight = std::max(1u, image.m_height >> mip);

        const size_t faceTexelCount = static_cast<size_t>(mipWidth) * static_cast<size_t>(mipHeight);

        const size_t mipStartTexelOffset = destinationTexelOffset;

        for (uint32_t face = 0; face < faceCount; ++face)
        {
            const uint32_t sourceMipOffset = mipOffsets[face][mip];

            if (sourceMipOffset >= image.m_dataSize)
            {
                std::cerr << "[EnvironmentBaker] Invalid CMFT mip offset. "
                          << "Face: " << face << ", mip: " << mip << ", offset: " << sourceMipOffset
                          << ", data size: " << image.m_dataSize << std::endl;

                return std::nullopt;
            }

            const uint8_t* sourceMipData = sourceBytes + sourceMipOffset;

            for (size_t texel = 0; texel < faceTexelCount; ++texel)
            {
                const uint8_t* sourcePixel = sourceMipData + texel * bytesPerPixel;

                float rgba32f[4]{};

                cmft::toRgba32f(rgba32f, image.m_format, sourcePixel);

                const size_t destination = (destinationTexelOffset + texel) * 4u;

                rgba16fData[destination + 0] = glm::packHalf1x16(rgba32f[0]);

                rgba16fData[destination + 1] = glm::packHalf1x16(rgba32f[1]);

                rgba16fData[destination + 2] = glm::packHalf1x16(rgba32f[2]);

                rgba16fData[destination + 3] = glm::packHalf1x16(rgba32f[3]);
            }

            destinationTexelOffset += faceTexelCount;
        }

        mipChain.push_back(texture_compiler::ImageSizedData{
            .pixels = reinterpret_cast<const uint8_t*>(rgba16fData.data() + mipStartTexelOffset * 4u),

            .width = mipWidth,

            .height = mipHeight});
    }

    const texture_compiler::TextureCompileOptions options = makeEnvironmentTextureOptions();

    const texture_format::ImageViewType imageViewType =
        faceCount == 6 ? texture_format::ImageViewType::ViewCube : texture_format::ImageViewType::View2DArray;

    auto compressed = texture_compiler::TextureCompiler::compileRGBA16F(
        std::span<const texture_compiler::ImageSizedData>(mipChain.data(), mipChain.size()), options, faceCount,
        imageViewType);

    if (!compressed)
    {
        std::cerr << "[EnvironmentBaker] Failed to compress CMFT image to BC6H." << std::endl;

        return std::nullopt;
    }

    return compressed;
}
} // namespace

std::optional<CompiledEnvironment> EnvironmentBaker::bake(const std::filesystem::path& hdrFile)
{
    cmft::Image hdrImage{};
    cmft::Image skybox{};
    cmft::Image irradiance{};
    cmft::Image radiance{};

    CmftImageGuard hdrGuard{&hdrImage, allocator()};

    CmftImageGuard skyboxGuard{&skybox, allocator()};

    CmftImageGuard irradianceGuard{&irradiance, allocator()};

    CmftImageGuard radianceGuard{&radiance, allocator()};

    bool ok = cmft::imageLoad(hdrImage, hdrFile.string().c_str(), cmft::TextureFormat::Null, allocator());

    if (!ok)
    {
        std::cerr << "[EnvironmentBaker] Failed to load HDR file: " << hdrFile << std::endl;

        return std::nullopt;
    }

    std::cout << "[EnvironmentBaker] HDR loaded." << std::endl;

    ok = cmft::imageCubemapFromLatLong(skybox, hdrImage, true, allocator());

    if (!ok)
    {
        std::cerr << "[EnvironmentBaker] Failed to generate cubemap." << std::endl;

        return std::nullopt;
    }

    std::cout << "[EnvironmentBaker] Skybox cubemap generated." << std::endl;

    ok = cmft::imageIrradianceFilterSh(irradiance, 32, skybox, allocator());

    if (!ok)
    {
        std::cerr << "[EnvironmentBaker] Failed to generate irradiance map." << std::endl;

        return std::nullopt;
    }

    std::cout << "[EnvironmentBaker] Irradiance map generated." << std::endl;

    ok = cmft::imageRadianceFilter(radiance, 128, cmft::LightingModel::BlinnBrdf, false, 8, 10, 0, skybox,
                                   cmft::EdgeFixup::Warp, makeCmftThreadCount(), nullptr, allocator());

    if (!ok)
    {
        std::cerr << "[EnvironmentBaker] Failed to generate prefiltered radiance map." << std::endl;

        return std::nullopt;
    }

    std::cout << "[EnvironmentBaker] Prefiltered radiance map generated." << std::endl;

    std::cout << "[EnvironmentBaker] Compressing environment maps to BC6H..." << std::endl;

    auto skyboxTexture = convertCmftImageToBc6h(skybox);

    auto irradianceTexture = convertCmftImageToBc6h(irradiance);

    auto radianceTexture = convertCmftImageToBc6h(radiance);

    if (!skyboxTexture || !irradianceTexture || !radianceTexture)
    {
        std::cerr << "[EnvironmentBaker] Failed to convert environment textures to BC6H." << std::endl;

        return std::nullopt;
    }

    CompiledEnvironment result{};

    result.textures.reserve(3);

    result.textures.push_back(std::move(*skyboxTexture));

    result.textures.push_back(std::move(*irradianceTexture));

    result.textures.push_back(std::move(*radianceTexture));

    auto& info = result.info;

    info.nameHash = 0;

    info.skyboxTextureIndex = 0;

    info.irradianceTextureIndex = 1;

    info.prefilteredTextureIndex = 2;

    info.intensity = 1.0f;

    info.skyboxIntensity = 1.0f;

    info.rotationYRadians = 0.0f;

    info.flags = static_cast<uint32_t>(environment_format::EnvironmentFlags::VisibleSkybox);

    info.tint = glm::vec4(1.0f);

    std::cout << "[EnvironmentBaker] Environment baked successfully." << std::endl;

    std::cout << "  Skybox bytes:     " << result.textures[0].data.size() << '\n';

    std::cout << "  Irradiance bytes: " << result.textures[1].data.size() << '\n';

    std::cout << "  Radiance bytes:   " << result.textures[2].data.size() << '\n';

    return result;
}
} // namespace shuttle::assets::environment_compiler