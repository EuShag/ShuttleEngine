#include "SceneTextureCompiler.hpp"

#include <Assets/TextureCompiler/ImageData.hpp>
#include <Assets/TextureCompiler/TextureCompiler.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>
#include <vector>

#include <mio/mio.hpp>

#include <stb_image.h>
#include <stb_image_resize2.h>

#include "Intermediate/ImportedScene.hpp"

namespace shuttle::assets::texture_compiler
{
struct CompiledTexture;
}

namespace shuttle::assets::scene_compiler
{
namespace
{
namespace texture_format = shuttle::assets::formats::texture;

void markRequiredTexture(const ImportedScene& scene, std::vector<uint8_t>& required, int32_t textureIndex)
{
    if (textureIndex < 0 || textureIndex >= static_cast<int32_t>(scene.textures.size()))
    {
        return;
    }

    required[static_cast<size_t>(textureIndex)] = 1;
}

std::vector<uint8_t> collectRequiredTextures(const ImportedScene& scene)
{
    std::vector<uint8_t> required;
    required.resize(scene.textures.size(), 0);

    for (const ImportedMaterial& material : scene.materials)
    {
        markRequiredTexture(scene, required, material.albedoTexture);

        markRequiredTexture(scene, required, material.normalTexture);

        markRequiredTexture(scene, required, material.emissiveTexture);

        if (material.ormTexture != InvalidIndexI32)
        {
            markRequiredTexture(scene, required, material.ormTexture);
        }
        else
        {
            markRequiredTexture(scene, required, material.occlusionTexture);

            markRequiredTexture(scene, required, material.roughnessTexture);

            markRequiredTexture(scene, required, material.metallicTexture);
        }
    }

    return required;
}

struct RgbaImage
{
    std::vector<uint8_t> pixels;

    uint32_t width = 0;
    uint32_t height = 0;

    [[nodiscard]]
    bool valid() const noexcept
    {
        return !pixels.empty() && width > 0 && height > 0;
    }
};

texture_compiler::TextureCompileOptions makeOptionsForTexture(const ImportedTexture& texture,
                                                              const SceneTextureCompilerOptions& sceneOptions)
{
    texture_compiler::TextureCompileOptions options{};

    options.semantic = texture.semantic;

    options.generateMips = sceneOptions.generateMips;

    options.flipY = sceneOptions.flipY;

    options.roughnessIsGloss = sceneOptions.roughnessIsGloss;

    switch (texture.semantic)
    {
    case texture_format::TextureSemantic::Albedo:
    case texture_format::TextureSemantic::Emissive: options.format = VK_FORMAT_BC7_SRGB_BLOCK; break;

    case texture_format::TextureSemantic::Normal: options.format = VK_FORMAT_BC5_UNORM_BLOCK; break;

    case texture_format::TextureSemantic::ORM: options.format = VK_FORMAT_BC7_UNORM_BLOCK; break;

    case texture_format::TextureSemantic::IBL: options.format = VK_FORMAT_BC6H_UFLOAT_BLOCK; break;

    case texture_format::TextureSemantic::Unknown:
    default: options.format = VK_FORMAT_BC7_UNORM_BLOCK; break;
    }

    return options;
}

texture_compiler::TextureCompileOptions makeOrmOptions(const SceneTextureCompilerOptions& sceneOptions)
{
    texture_compiler::TextureCompileOptions options{};

    options.semantic = texture_format::TextureSemantic::ORM;

    options.format = VK_FORMAT_BC7_UNORM_BLOCK;

    options.generateMips = sceneOptions.generateMips;

    options.flipY = false;

    options.roughnessIsGloss = sceneOptions.roughnessIsGloss;

    return options;
}

std::optional<texture_compiler::CompiledTexture> compileImportedTexture(const ImportedTexture& texture,
                                                                        const SceneTextureCompilerOptions& sceneOptions)
{
    const texture_compiler::TextureCompileOptions options = makeOptionsForTexture(texture, sceneOptions);

    switch (texture.sourceKind)
    {
    case ImportedTextureSourceKind::File:
    {
        if (texture.sourcePath.empty())
        {
            return std::nullopt;
        }

        return texture_compiler::TextureCompiler::compileFile(texture.sourcePath, options);
    }

    case ImportedTextureSourceKind::EmbeddedEncoded:
    {
        if (texture.embeddedBytes.empty())
        {
            return std::nullopt;
        }

        return texture_compiler::TextureCompiler::compileMemory(
            texture_compiler::ImageData{.pixels = texture.embeddedBytes.data(), .size = texture.embeddedBytes.size()},
            texture.formatHint, options);
    }

    case ImportedTextureSourceKind::EmbeddedRGBA:
    {
        if (texture.embeddedBytes.empty() || texture.width == 0 || texture.height == 0)
        {
            return std::nullopt;
        }

        return texture_compiler::TextureCompiler::compileFromRGBA(texture.embeddedBytes.data(), texture.width,
                                                                  texture.height, options);
    }

    case ImportedTextureSourceKind::Generated:
    case ImportedTextureSourceKind::None:
    default: return std::nullopt;
    }
}

std::optional<RgbaImage> loadRgbaImageFromTexture(const ImportedTexture& texture)
{
    switch (texture.sourceKind)
    {
    case ImportedTextureSourceKind::File:
    {
        if (texture.sourcePath.empty())
        {
            return std::nullopt;
        }

        mio::mmap_source mapping;
        std::error_code error;

        mapping.map(texture.sourcePath.string(), error);

        if (error || mapping.empty())
        {
            return std::nullopt;
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        uint8_t* rawPixels = stbi_load_from_memory(reinterpret_cast<const uint8_t*>(mapping.data()),
                                                   static_cast<int>(mapping.size()), &width, &height, &channels, 4);

        if (!rawPixels)
        {
            return std::nullopt;
        }

        RgbaImage image{};
        image.width = static_cast<uint32_t>(width);

        image.height = static_cast<uint32_t>(height);

        image.pixels.assign(rawPixels, rawPixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

        stbi_image_free(rawPixels);

        return image;
    }

    case ImportedTextureSourceKind::EmbeddedEncoded:
    {
        if (texture.embeddedBytes.empty())
        {
            return std::nullopt;
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        uint8_t* rawPixels =
            stbi_load_from_memory(texture.embeddedBytes.data(), static_cast<int>(texture.embeddedBytes.size()), &width,
                                  &height, &channels, 4);

        if (!rawPixels)
        {
            return std::nullopt;
        }

        RgbaImage image{};
        image.width = static_cast<uint32_t>(width);

        image.height = static_cast<uint32_t>(height);

        image.pixels.assign(rawPixels, rawPixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

        stbi_image_free(rawPixels);

        return image;
    }

    case ImportedTextureSourceKind::EmbeddedRGBA:
    {
        if (texture.embeddedBytes.empty() || texture.width == 0 || texture.height == 0)
        {
            return std::nullopt;
        }

        RgbaImage image{};
        image.width = texture.width;

        image.height = texture.height;

        image.pixels = texture.embeddedBytes;

        return image;
    }

    case ImportedTextureSourceKind::Generated:
    case ImportedTextureSourceKind::None:
    default: return std::nullopt;
    }
}

RgbaImage resizeOrFill(const std::optional<RgbaImage>& source, uint8_t fallbackValue, uint32_t targetWidth,
                       uint32_t targetHeight)
{
    RgbaImage result{};
    result.width = targetWidth;

    result.height = targetHeight;

    result.pixels.resize(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u);

    if (!source || !source->valid())
    {
        std::ranges::fill(result.pixels, fallbackValue);

        return result;
    }

    if (source->width == targetWidth && source->height == targetHeight)
    {
        result.pixels = source->pixels;

        return result;
    }

    stbir_resize_uint8_linear(source->pixels.data(), static_cast<int>(source->width), static_cast<int>(source->height),
                              0, result.pixels.data(), static_cast<int>(targetWidth), static_cast<int>(targetHeight), 0,
                              STBIR_RGBA);

    return result;
}

bool validTextureIndex(const ImportedScene& scene, int32_t textureIndex)
{
    return textureIndex >= 0 && textureIndex < static_cast<int32_t>(scene.textures.size());
}

std::optional<texture_compiler::CompiledTexture> buildOrmTexture(const ImportedScene& scene,
                                                                 const ImportedMaterial& material,
                                                                 const SceneTextureCompilerOptions& sceneOptions)
{
    std::optional<RgbaImage> occlusionImage;
    std::optional<RgbaImage> roughnessImage;
    std::optional<RgbaImage> metallicImage;

    if (validTextureIndex(scene, material.occlusionTexture))
    {
        occlusionImage = loadRgbaImageFromTexture(scene.textures[static_cast<size_t>(material.occlusionTexture)]);
    }

    if (validTextureIndex(scene, material.roughnessTexture))
    {
        roughnessImage = loadRgbaImageFromTexture(scene.textures[static_cast<size_t>(material.roughnessTexture)]);
    }

    if (validTextureIndex(scene, material.metallicTexture))
    {
        metallicImage = loadRgbaImageFromTexture(scene.textures[static_cast<size_t>(material.metallicTexture)]);
    }

    if (!occlusionImage && !roughnessImage && !metallicImage)
    {
        return std::nullopt;
    }

    uint32_t targetWidth = 1;
    uint32_t targetHeight = 1;

    auto includeSize = [&targetWidth, &targetHeight](const std::optional<RgbaImage>& image)
    {
        if (!image || !image->valid())
        {
            return;
        }

        targetWidth = std::max(targetWidth, image->width);

        targetHeight = std::max(targetHeight, image->height);
    };

    includeSize(occlusionImage);
    includeSize(roughnessImage);
    includeSize(metallicImage);

    RgbaImage occlusion = resizeOrFill(occlusionImage, 255, targetWidth, targetHeight);

    RgbaImage roughness = resizeOrFill(roughnessImage, 255, targetWidth, targetHeight);

    RgbaImage metallic = resizeOrFill(metallicImage, 0, targetWidth, targetHeight);

    std::vector<uint8_t> ormPixels(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4u);

    const bool roughnessIsGloss = sceneOptions.roughnessIsGloss;

    const size_t pixelCount = static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight);

    for (size_t pixel = 0; pixel < pixelCount; ++pixel)
    {
        const size_t offset = pixel * 4u;

        const uint8_t ao = occlusion.pixels[offset + 0];

        uint8_t rough = roughness.pixels[offset + 0];

        const uint8_t metal = metallic.pixels[offset + 0];

        if (roughnessIsGloss)
        {
            rough = static_cast<uint8_t>(255u - rough);
        }

        ormPixels[offset + 0] = ao;
        ormPixels[offset + 1] = rough;
        ormPixels[offset + 2] = metal;
        ormPixels[offset + 3] = 255;
    }

    const texture_compiler::TextureCompileOptions ormOptions = makeOrmOptions(sceneOptions);

    return texture_compiler::TextureCompiler::compileFromRGBA(ormPixels.data(), targetWidth, targetHeight, ormOptions);
}

bool materialNeedsGeneratedOrm(const ImportedMaterial& material)
{
    if (material.ormTexture != InvalidIndexI32)
    {
        return false;
    }

    return material.occlusionTexture != InvalidIndexI32 || material.roughnessTexture != InvalidIndexI32 ||
           material.metallicTexture != InvalidIndexI32;
}

std::string makeGeneratedOrmName(size_t materialIndex, const ImportedMaterial& material)
{
    if (!material.name.empty())
    {
        return material.name + "_GeneratedORM";
    }

    return "Material_" + std::to_string(materialIndex) + "_GeneratedORM";
}
} // namespace

SceneTextureCompilerResult SceneTextureCompiler::compile(ImportedScene& scene,
                                                         const SceneTextureCompilerOptions& options)
{
    SceneTextureCompilerResult result{};

    if (!options.compileTextures)
    {
        result.importedToCompiledTexture.resize(scene.textures.size(), InvalidIndexI32);


        return result;
    }

    result.importedToCompiledTexture.resize(scene.textures.size(), InvalidIndexI32);

    // ==========================================================
    // 1. Generate ORM first
    // ==========================================================

    if (options.generateOrmTextures)
    {
        for (size_t materialIndex = 0; materialIndex < scene.materials.size(); ++materialIndex)
        {
            ImportedMaterial& material = scene.materials[materialIndex];

            if (!materialNeedsGeneratedOrm(material))
            {
                continue;
            }

            std::optional<texture_compiler::CompiledTexture> generatedOrm = buildOrmTexture(scene, material, options);

            if (!generatedOrm)
            {
                continue;
            }

            ImportedTexture generatedTexture{};
            generatedTexture.name = makeGeneratedOrmName(materialIndex, material);

            generatedTexture.sourceKind = ImportedTextureSourceKind::Generated;

            generatedTexture.semantic = texture_format::TextureSemantic::ORM;

            generatedTexture.formatHint = "generated-orm";

            const auto importedTextureIndex = static_cast<int32_t>(scene.textures.size());

            scene.textures.push_back(std::move(generatedTexture));

            const auto compiledTextureIndex = static_cast<int32_t>(result.textures.size());

            result.textures.push_back(std::move(*generatedOrm));

            result.importedToCompiledTexture.push_back(compiledTextureIndex);

            material.ormTexture = importedTextureIndex;
        }
    }

    // ==========================================================
    // 2. Collect only actually used textures
    // ==========================================================

    const std::vector<uint8_t> requiredTextures = collectRequiredTextures(scene);

    // ==========================================================
    // 3. Compile only required non-generated textures
    // ==========================================================

    for (size_t textureIndex = 0; textureIndex < scene.textures.size(); ++textureIndex)
    {
        if (textureIndex >= requiredTextures.size() || requiredTextures[textureIndex] == 0)
        {
            continue;
        }

        if (textureIndex < result.importedToCompiledTexture.size() &&
            result.importedToCompiledTexture[textureIndex] != InvalidIndexI32)
        {
            // Already compiled, usually generated ORM.
            continue;
        }

        const ImportedTexture& texture = scene.textures[textureIndex];

        if (texture.sourceKind == ImportedTextureSourceKind::Generated)
        {
            continue;
        }

        std::optional<texture_compiler::CompiledTexture> compiled = compileImportedTexture(texture, options);

        if (!compiled)
        {
            continue;
        }

        const auto compiledIndex = static_cast<int32_t>(result.textures.size());

        result.textures.push_back(std::move(*compiled));

        result.importedToCompiledTexture[textureIndex] = compiledIndex;
    }

    for (size_t i = 0; i < scene.materials.size(); ++i)
    {
        const auto& m = scene.materials[i];

        printf(
            "MAT %zu imported orm=%d\n",
            i,
            m.ormTexture);
    }

    return result;
}
} // namespace shuttle::assets::scene_compiler
