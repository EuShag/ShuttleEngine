#include "EnvironmentBaker.hpp"

#include <Assets/Formats/Environment.hpp>
#include <Assets/Formats/Texture.hpp>

#include <iostream>
#include <optional>
#include <vector>
#include <algorithm>
#include <Assets/TextureCompiler/ImageData.hpp>
#include <Assets/TextureCompiler/TextureCompileOptions.hpp>
#include <Assets/TextureCompiler/TextureCompiler.hpp>
#include <glm/gtc/packing.hpp>

#include "../include/Assets/EnvironmentCompiler/CpuIblGenerator.hpp"
#include "stb_image.h"

namespace shuttle::assets::environment_compiler
{
    namespace
    {
        uint16_t floatToHalf(float value)
        {
            return glm::packHalf1x16(value);
        }

        // Вспомогательная функция: генерирует только "чистые" сырые пиксели без опасных указателей
        std::vector<std::vector<uint16_t>> convertCubemapToFp16(engine::ibl::Cubemap const& cubemap)
        {
            std::vector<std::vector<uint16_t>> mipPixels(cubemap.mips.size());

            for (size_t mipIndex = 0; mipIndex < cubemap.mips.size(); ++mipIndex)
            {
                auto const& mip = cubemap.mips[mipIndex];
                size_t const pixelsPerFace = static_cast<size_t>(mip.size) * mip.size;

                std::vector<uint16_t>& pixels = mipPixels[mipIndex];
                pixels.resize(pixelsPerFace * 6u * 4u); // 6 граней, RGBA

                for (uint32_t face = 0; face < 6; ++face)
                {
                    auto const& facePixels = mip.faces[face];
                    size_t const faceOffset = static_cast<size_t>(face) * pixelsPerFace * 4u;

                    for (size_t i = 0; i < pixelsPerFace; ++i)
                    {
                        auto const& src = facePixels[i];
                        size_t const dstOffset = faceOffset + (i * 4u);

                        pixels[dstOffset + 0] = floatToHalf(std::max(src.x, 0.0f));
                        pixels[dstOffset + 1] = floatToHalf(std::max(src.y, 0.0f));
                        pixels[dstOffset + 2] = floatToHalf(std::max(src.z, 0.0f));
                        pixels[dstOffset + 3] = floatToHalf(src.w);
                    }
                }
            }

            return mipPixels;
        }
    }

    std::optional<CompiledEnvironment> EnvironmentBaker::bake(
        const std::filesystem::path& hdrFile,
        engine::ibl::IblGenerationSettings const& settings)
    {
        using namespace engine::ibl;
        namespace texture_format = formats::texture;

        int hdrWidth = 0;
        int hdrHeight = 0;
        int hdrChannels = 0;

        float* loadedHdrPixelsRGBA32F = stbi_loadf(
            hdrFile.string().c_str(),
            &hdrWidth,
            &hdrHeight,
            &hdrChannels,
            4);

        if (!loadedHdrPixelsRGBA32F)
        {
            std::cerr << "[EnvironmentBaker] Failed to load HDR: " << hdrFile << std::endl;
            return std::nullopt;
        }

        Image2D hdr{};
        hdr.width = static_cast<uint32_t>(hdrWidth);
        hdr.height = static_cast<uint32_t>(hdrHeight);
        hdr.pixels.resize(static_cast<size_t>(hdr.width) * hdr.height);

        for (uint32_t y = 0; y < hdr.height; ++y)
        {
            for (uint32_t x = 0; x < hdr.width; ++x)
            {
                size_t const srcOffset = (static_cast<size_t>(y) * hdr.width + x) * 4u;
                size_t const dstOffset = static_cast<size_t>(y) * hdr.width + x;

                hdr.pixels[dstOffset] = {
                    loadedHdrPixelsRGBA32F[srcOffset + 0],
                    loadedHdrPixelsRGBA32F[srcOffset + 1],
                    loadedHdrPixelsRGBA32F[srcOffset + 2],
                    loadedHdrPixelsRGBA32F[srcOffset + 3]
                };
            }
        }

        stbi_image_free(loadedHdrPixelsRGBA32F);

        // Генерация IBL (все мипы успешно создаются на CPU)
        GeneratedIbl ibl = generateIblFromEquirectangularHdr(hdr, settings);

        std::cout << "Radiance mips: " << ibl.radiance.mips.size() << std::endl;

        // Конвертируем кубмапы в сырые FP16 данные (безопасное владение памятью)
        auto skyboxPixels = convertCubemapToFp16(ibl.skybox);
        auto irradiancePixels = convertCubemapToFp16(ibl.irradiance);
        auto radiancePixels = convertCubemapToFp16(ibl.radiance);

        // Строим mipViews локально на стеке. Указатели гарантированно будут валидны при компиляции!
        auto buildMipViews = [](std::vector<std::vector<uint16_t>> const& mipPixels, Cubemap const& cubemap)
        {
            std::vector<texture_compiler::ImageSizedData> mipViews(cubemap.mips.size());
            for (size_t i = 0; i < cubemap.mips.size(); ++i)
            {
                mipViews[i] = texture_compiler::ImageSizedData{
                    .pixels = reinterpret_cast<const uint8_t*>(mipPixels[i].data()),
                    .width  = cubemap.mips[i].size,
                    .height = cubemap.mips[i].size
                };
            }
            return mipViews;
        };

        auto skyboxMipViews = buildMipViews(skyboxPixels, ibl.skybox);
        auto irradianceMipViews = buildMipViews(irradiancePixels, ibl.irradiance);
        auto radianceMipViews = buildMipViews(radiancePixels, ibl.radiance);

        texture_compiler::TextureCompileOptions bc6hOptions{};
        bc6hOptions.format = VK_FORMAT_BC6H_UFLOAT_BLOCK;
        bc6hOptions.generateMips = false; // Мы передаем уже готовые мипы!

        auto skyboxTexture = texture_compiler::TextureCompiler::compileRGBA16F(
            skyboxMipViews,
            bc6hOptions,
            6,
            texture_format::ImageViewType::ViewCube);

        auto irradianceTexture = texture_compiler::TextureCompiler::compileRGBA16F(
            irradianceMipViews,
            bc6hOptions,
            6,
            texture_format::ImageViewType::ViewCube);

        auto radianceTexture = texture_compiler::TextureCompiler::compileRGBA16F(
            radianceMipViews,
            bc6hOptions,
            6,
            texture_format::ImageViewType::ViewCube);

        // ИСПРАВЛЕНО: Добавлены операторы логического ИЛИ (||)
        if (!skyboxTexture || !irradianceTexture || !radianceTexture)
        {
            std::cerr << "[EnvironmentBaker] Failed to compile BC6H environment textures." << std::endl;
            return std::nullopt;
        }

        CompiledEnvironment result{};

        result.textures.push_back(std::move(*skyboxTexture));
        result.textures.push_back(std::move(*irradianceTexture));
        result.textures.push_back(std::move(*radianceTexture));

        result.info.skyboxTextureIndex = 0;
        result.info.irradianceTextureIndex = 1;
        result.info.prefilteredTextureIndex = 2;

        result.info.intensity = 1.0f;
        result.info.skyboxIntensity = 1.0f;
        result.info.rotationYRadians = 0.0f;
        result.info.prefilteredTextureMipLevels = ibl.radiance.mips.size(); // Сохраняем правильное число мипов!

        return result;
    }
} // namespace shuttle::assets::environment_compiler