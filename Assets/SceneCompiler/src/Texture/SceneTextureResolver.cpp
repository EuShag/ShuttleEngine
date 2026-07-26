#include "SceneTextureResolver.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shuttle::assets::scene_compiler
{
    namespace
    {
        namespace texture_format = shuttle::assets::formats::texture;

        struct TextureCatalogItem
        {
            std::filesystem::path path;
            std::string lowerName;
        };

        struct ResolverContext
        {
            ImportedScene* scene = nullptr;

            std::filesystem::path sourceDirectory;

            std::vector<TextureCatalogItem> catalog;

            std::unordered_map<std::string, int32_t> texturePathToIndex;
        };

        constexpr std::array<std::string_view, 5> AlbedoTokens =
        {
            "basecolor",
            "base_color",
            "albedo",
            "diffuse",
            "color"
        };

        constexpr std::array<std::string_view, 3> NormalTokens =
        {
            "normal",
            "nrm",
            "nor"
        };

        constexpr std::array<std::string_view, 4> OrmTokens =
        {
            "orm",
            "arm",
            "rma",
            "mra"
        };

        constexpr std::array<std::string_view, 5> OcclusionTokens =
        {
            "ao",
            "_ao",
            "occlusion",
            "ambientocclusion",
            "ambient_occlusion"
        };

        constexpr std::array<std::string_view, 2> RoughnessTokens =
        {
            "rough",
            "roughness"
        };

        constexpr std::array<std::string_view, 3> MetallicTokens =
        {
            "metal",
            "metallic",
            "metalness"
        };

        constexpr std::array<std::string_view, 3> EmissiveTokens =
        {
            "emissive",
            "emission",
            "emit"
        };

        constexpr std::array<std::string_view, 13> NonAlbedoTokens =
        {
            "specular",
            "_spec",
            "roughness",
            "rough",
            "metallic",
            "metalness",
            "metal",
            "normal",
            "nrm",
            "_ao",
            "occlusion",
            "height",
            "emissive"
        };

        constexpr std::array<std::string_view, 9> NonNormalTokens =
        {
            "basecolor",
            "base_color",
            "albedo",
            "diffuse",
            "specular",
            "roughness",
            "metallic",
            "metalness",
            "emissive"
        };

        constexpr std::array<std::string_view, 7> NonOrmTokens =
        {
            "basecolor",
            "base_color",
            "albedo",
            "diffuse",
            "normal",
            "nrm",
            "emissive"
        };

        constexpr std::array<std::string_view, 9> NonOcclusionTokens =
        {
            "albedo",
            "basecolor",
            "base_color",
            "normal",
            "roughness",
            "rough",
            "metallic",
            "metalness",
            "emissive"
        };

        constexpr std::array<std::string_view, 7> NonRoughnessTokens =
        {
            "albedo",
            "basecolor",
            "base_color",
            "normal",
            "metallic",
            "metalness",
            "emissive"
        };

        constexpr std::array<std::string_view, 6> NonMetallicTokens =
        {
            "albedo",
            "basecolor",
            "base_color",
            "normal",
            "roughness",
            "emissive"
        };

        std::string toLower(
            std::string text)
        {
            for (char& c : text)
            {
                c =
                    static_cast<char>(
                        std::tolower(
                            static_cast<unsigned char>(c)));
            }

            return text;
        }

        std::string normalizePathKey(
            const std::filesystem::path& path)
        {
            std::string value =
                path.lexically_normal()
                    .generic_string();

            return toLower(value);
        }

        bool hasKnownTextureExtension(
            const std::filesystem::path& path)
        {
            const std::string extension =
                toLower(
                    path.extension()
                        .string());

            return
                extension == ".dds" ||
                extension == ".png" ||
                extension == ".jpg" ||
                extension == ".jpeg" ||
                extension == ".tga" ||
                extension == ".bmp" ||
                extension == ".hdr" ||
                extension == ".exr";
        }

        bool containsAny(
            std::string_view text,
            std::span<const std::string_view> tokens)
        {
            for (std::string_view token : tokens)
            {
                if (text.find(token) != std::string_view::npos)
                {
                    return true;
                }
            }

            return false;
        }

        void collectTokens(
            const std::string& text,
            std::vector<std::string>& outTokens)
        {
            std::string current;

            for (char c : text)
            {
                if (std::isalnum(
                        static_cast<unsigned char>(c)))
                {
                    current.push_back(
                        static_cast<char>(
                            std::tolower(
                                static_cast<unsigned char>(c))));
                }
                else
                {
                    if (current.size() >= 4)
                    {
                        outTokens.push_back(current);
                    }

                    current.clear();
                }
            }

            if (current.size() >= 4)
            {
                outTokens.push_back(current);
            }
        }

        std::vector<std::string> collectMaterialTokens(
            const ImportedScene& scene,
            const ImportedMaterial& material)
        {
            std::vector<std::string> result;

            collectTokens(
                toLower(material.name),
                result);

            auto collectTextureName =
                [&](int32_t textureIndex)
                {
                    if (textureIndex < 0 ||
                        textureIndex >= static_cast<int32_t>(scene.textures.size()))
                    {
                        return;
                    }

                    const ImportedTexture& texture =
                        scene.textures[static_cast<size_t>(textureIndex)];

                    if (!texture.name.empty())
                    {
                        collectTokens(
                            toLower(texture.name),
                            result);
                    }

                    if (!texture.sourcePath.empty())
                    {
                        collectTokens(
                            toLower(
                                texture.sourcePath
                                    .filename()
                                    .string()),
                            result);
                    }
                };

            collectTextureName(material.albedoTexture);
            collectTextureName(material.normalTexture);
            collectTextureName(material.ormTexture);
            collectTextureName(material.occlusionTexture);
            collectTextureName(material.roughnessTexture);
            collectTextureName(material.metallicTexture);
            collectTextureName(material.emissiveTexture);

            return result;
        }

        void buildInitialTextureMap(
            ResolverContext& context)
        {
            ImportedScene& scene =
                *context.scene;

            for (size_t i = 0;
                 i < scene.textures.size();
                 ++i)
            {
                const ImportedTexture& texture =
                    scene.textures[i];

                if (texture.sourceKind != ImportedTextureSourceKind::File ||
                    texture.sourcePath.empty())
                {
                    continue;
                }

                context.texturePathToIndex[
                    normalizePathKey(texture.sourcePath)] =
                        static_cast<int32_t>(i);
            }
        }

        void scanTextureCatalog(
            ResolverContext& context)
        {
            if (context.sourceDirectory.empty() ||
                !std::filesystem::exists(context.sourceDirectory))
            {
                return;
            }

            try
            {
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(context.sourceDirectory))
                {
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }

                    const std::filesystem::path path =
                        entry.path();

                    if (!hasKnownTextureExtension(path))
                    {
                        continue;
                    }

                    TextureCatalogItem item{};
                    item.path =
                        path.lexically_normal();

                    item.lowerName =
                        toLower(
                            path.filename()
                                .string());

                    context.catalog.push_back(
                        std::move(item));
                }
            }
            catch (...)
            {
            }
        }

        void setTextureSemanticIfUnknown(
            ImportedScene& scene,
            int32_t textureIndex,
            texture_format::TextureSemantic semantic)
        {
            if (textureIndex < 0 ||
                textureIndex >= static_cast<int32_t>(scene.textures.size()))
            {
                return;
            }

            ImportedTexture& texture =
                scene.textures[static_cast<size_t>(textureIndex)];

            if (texture.semantic == texture_format::TextureSemantic::Unknown)
            {
                texture.semantic =
                    semantic;
            }
        }

        int32_t addFileTexture(
            ResolverContext& context,
            const std::filesystem::path& rawPath,
            texture_format::TextureSemantic semantic)
        {
            std::filesystem::path path =
                rawPath;

            if (path.is_relative())
            {
                path =
                    context.sourceDirectory /
                    path;
            }

            path =
                path.lexically_normal();

            const std::string key =
                normalizePathKey(path);

            if (const auto it = context.texturePathToIndex.find(key);
                it != context.texturePathToIndex.end())
            {
                setTextureSemanticIfUnknown(
                    *context.scene,
                    it->second,
                    semantic);

                return it->second;
            }

            if (!std::filesystem::exists(path))
            {
                return InvalidIndexI32;
            }

            ImportedTexture texture{};
            texture.name =
                path.filename()
                    .string();

            texture.sourceKind =
                ImportedTextureSourceKind::File;

            texture.sourcePath =
                path;

            texture.formatHint =
                toLower(
                    path.extension()
                        .string());

            if (!texture.formatHint.empty() &&
                texture.formatHint.front() == '.')
            {
                texture.formatHint.erase(
                    texture.formatHint.begin());
            }

            texture.semantic =
                semantic;

            const int32_t index =
                static_cast<int32_t>(
                    context.scene->textures.size());

            context.scene->textures.push_back(
                std::move(texture));

            context.texturePathToIndex[key] =
                index;

            return index;
        }

        int32_t findBestInCatalog(
            ResolverContext& context,
            const std::vector<std::string>& materialTokens,
            std::span<const std::string_view> includeTokens,
            std::span<const std::string_view> excludeTokens,
            texture_format::TextureSemantic semantic)
        {
            int bestScore =
                -1'000'000;

            std::filesystem::path bestPath;

            for (const TextureCatalogItem& item : context.catalog)
            {
                const std::string& name =
                    item.lowerName;

                if (!containsAny(name, includeTokens))
                {
                    continue;
                }

                if (containsAny(name, excludeTokens))
                {
                    continue;
                }

                int score = 100;

                for (const std::string& token : materialTokens)
                {
                    if (!token.empty() &&
                        name.find(token) != std::string::npos)
                    {
                        score += 25;
                    }
                }

                if (score > bestScore)
                {
                    bestScore =
                        score;

                    bestPath =
                        item.path;
                }
            }

            if (bestPath.empty())
            {
                return InvalidIndexI32;
            }

            return addFileTexture(
                context,
                bestPath,
                semantic);
        }

        std::string replaceFirstToken(
            std::string value,
            std::string_view from,
            std::string_view to)
        {
            const size_t pos =
                value.find(from);

            if (pos == std::string::npos)
            {
                return {};
            }

            value.replace(
                pos,
                from.size(),
                to);

            return value;
        }

        int32_t inferFromSibling(
            ResolverContext& context,
            int32_t referenceTextureIndex,
            std::span<const std::string_view> sourceTokens,
            std::span<const std::string_view> targetTokens,
            texture_format::TextureSemantic semantic)
        {
            ImportedScene& scene =
                *context.scene;

            if (referenceTextureIndex < 0 ||
                referenceTextureIndex >= static_cast<int32_t>(scene.textures.size()))
            {
                return InvalidIndexI32;
            }

            const ImportedTexture& referenceTexture =
                scene.textures[static_cast<size_t>(referenceTextureIndex)];

            if (referenceTexture.sourceKind != ImportedTextureSourceKind::File ||
                referenceTexture.sourcePath.empty())
            {
                return InvalidIndexI32;
            }

            const std::filesystem::path referencePath =
                referenceTexture.sourcePath.lexically_normal();

            const std::filesystem::path parentPath =
                referencePath.parent_path();

            const std::string filename =
                referencePath.filename()
                    .string();

            const std::string lowerFilename =
                toLower(filename);

            for (std::string_view sourceToken : sourceTokens)
            {
                if (lowerFilename.find(sourceToken) == std::string::npos)
                {
                    continue;
                }

                for (std::string_view targetToken : targetTokens)
                {
                    const std::string candidateLowerName =
                        replaceFirstToken(
                            lowerFilename,
                            sourceToken,
                            targetToken);

                    if (candidateLowerName.empty())
                    {
                        continue;
                    }

                    for (const TextureCatalogItem& item : context.catalog)
                    {
                        if (item.path.parent_path() != parentPath)
                        {
                            continue;
                        }

                        if (item.lowerName == candidateLowerName)
                        {
                            return addFileTexture(
                                context,
                                item.path,
                                semantic);
                        }
                    }

                    std::filesystem::path directCandidate =
                        parentPath /
                        candidateLowerName;

                    if (std::filesystem::exists(directCandidate))
                    {
                        return addFileTexture(
                            context,
                            directCandidate,
                            semantic);
                    }
                }
            }

            return InvalidIndexI32;
        }

        int32_t resolveFromSiblingSet(
            ResolverContext& context,
            const ImportedMaterial& material,
            std::span<const std::string_view> targetTokens,
            texture_format::TextureSemantic semantic)
        {
            const std::array<int32_t, 6> references =
            {
                material.albedoTexture,
                material.normalTexture,
                material.ormTexture,
                material.occlusionTexture,
                material.roughnessTexture,
                material.metallicTexture
            };

            const std::array<std::span<const std::string_view>, 6> sourceTokenSets =
            {
                std::span<const std::string_view>(AlbedoTokens),
                std::span<const std::string_view>(NormalTokens),
                std::span<const std::string_view>(OrmTokens),
                std::span<const std::string_view>(OcclusionTokens),
                std::span<const std::string_view>(RoughnessTokens),
                std::span<const std::string_view>(MetallicTokens)
            };

            for (size_t i = 0;
                 i < references.size();
                 ++i)
            {
                const int32_t result =
                    inferFromSibling(
                        context,
                        references[i],
                        sourceTokenSets[i],
                        targetTokens,
                        semantic);

                if (result != InvalidIndexI32)
                {
                    return result;
                }
            }

            return InvalidIndexI32;
        }

        void resolveAlbedo(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.albedoTexture != InvalidIndexI32)
            {
                setTextureSemanticIfUnknown(
                    *context.scene,
                    material.albedoTexture,
                    texture_format::TextureSemantic::Albedo);

                return;
            }

            material.albedoTexture =
                resolveFromSiblingSet(
                    context,
                    material,
                    std::span<const std::string_view>(AlbedoTokens),
                    texture_format::TextureSemantic::Albedo);

            if (material.albedoTexture != InvalidIndexI32)
            {
                return;
            }

            material.albedoTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(AlbedoTokens),
                    std::span<const std::string_view>(NonAlbedoTokens),
                    texture_format::TextureSemantic::Albedo);
        }

        void resolveNormal(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.normalTexture != InvalidIndexI32)
            {
                setTextureSemanticIfUnknown(
                    *context.scene,
                    material.normalTexture,
                    texture_format::TextureSemantic::Normal);

                return;
            }

            material.normalTexture =
                resolveFromSiblingSet(
                    context,
                    material,
                    std::span<const std::string_view>(NormalTokens),
                    texture_format::TextureSemantic::Normal);

            if (material.normalTexture != InvalidIndexI32)
            {
                return;
            }

            material.normalTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(NormalTokens),
                    std::span<const std::string_view>(NonNormalTokens),
                    texture_format::TextureSemantic::Normal);
        }

        void resolveOrm(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.ormTexture != InvalidIndexI32)
            {
                setTextureSemanticIfUnknown(
                    *context.scene,
                    material.ormTexture,
                    texture_format::TextureSemantic::ORM);

                return;
            }

            material.ormTexture =
                resolveFromSiblingSet(
                    context,
                    material,
                    std::span<const std::string_view>(OrmTokens),
                    texture_format::TextureSemantic::ORM);

            if (material.ormTexture != InvalidIndexI32)
            {
                return;
            }

            material.ormTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(OrmTokens),
                    std::span<const std::string_view>(NonOrmTokens),
                    texture_format::TextureSemantic::ORM);
        }

        void resolveOcclusion(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.occlusionTexture != InvalidIndexI32)
            {
                return;
            }

            material.occlusionTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(OcclusionTokens),
                    std::span<const std::string_view>(NonOcclusionTokens),
                    texture_format::TextureSemantic::ORM);
        }

        void resolveRoughness(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.roughnessTexture != InvalidIndexI32)
            {
                return;
            }

            material.roughnessTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(RoughnessTokens),
                    std::span<const std::string_view>(NonRoughnessTokens),
                    texture_format::TextureSemantic::ORM);
        }

        void resolveMetallic(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.metallicTexture != InvalidIndexI32)
            {
                return;
            }

            material.metallicTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(MetallicTokens),
                    std::span<const std::string_view>(NonMetallicTokens),
                    texture_format::TextureSemantic::ORM);
        }

        void resolveEmissive(
            ResolverContext& context,
            ImportedMaterial& material,
            const std::vector<std::string>& materialTokens)
        {
            if (material.emissiveTexture != InvalidIndexI32)
            {
                setTextureSemanticIfUnknown(
                    *context.scene,
                    material.emissiveTexture,
                    texture_format::TextureSemantic::Emissive);

                return;
            }

            material.emissiveTexture =
                findBestInCatalog(
                    context,
                    materialTokens,
                    std::span<const std::string_view>(EmissiveTokens),
                    {},
                    texture_format::TextureSemantic::Emissive);
        }
    }

    void SceneTextureResolver::resolve(
        ImportedScene& scene,
        const std::filesystem::path& sourceDirectory,
        const SceneTextureResolverOptions& options)
    {
        ResolverContext context{};
        context.scene =
            &scene;

        context.sourceDirectory =
            sourceDirectory;

        buildInitialTextureMap(
            context);

        if (options.scanSourceDirectory)
        {
            scanTextureCatalog(
                context);
        }

        for (ImportedMaterial& material : scene.materials)
        {
            const std::vector<std::string> materialTokens =
                collectMaterialTokens(
                    scene,
                    material);

            if (options.resolveAlbedo)
            {
                resolveAlbedo(
                    context,
                    material,
                    materialTokens);
            }

            if (options.resolveNormal)
            {
                resolveNormal(
                    context,
                    material,
                    materialTokens);
            }

            if (options.resolveOrm)
            {
                resolveOrm(
                    context,
                    material,
                    materialTokens);
            }

            if (material.ormTexture == InvalidIndexI32)
            {
                if (options.resolveOcclusion)
                {
                    resolveOcclusion(
                        context,
                        material,
                        materialTokens);
                }

                if (options.resolveRoughness)
                {
                    resolveRoughness(
                        context,
                        material,
                        materialTokens);
                }

                if (options.resolveMetallic)
                {
                    resolveMetallic(
                        context,
                        material,
                        materialTokens);
                }
            }

            if (options.resolveEmissiveFromCatalog)
            {
                resolveEmissive(
                    context,
                    material,
                    materialTokens);
            }
            else if (material.emissiveTexture != InvalidIndexI32)
            {
                setTextureSemanticIfUnknown(
                    scene,
                    material.emissiveTexture,
                    texture_format::TextureSemantic::Emissive);
            }
        }
    }
}