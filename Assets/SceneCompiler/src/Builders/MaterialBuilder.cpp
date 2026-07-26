#include "MaterialBuilder.hpp"

#include "Assets/Formats/Material.hpp"
#include "Texture/SceneTextureCompiler.hpp"

namespace shuttle::assets::scene_compiler
{
    namespace
    {
        using namespace formats::material;

        uint32_t resolveTexture(
            const SceneTextureCompilerResult& textures,
            int32_t importedIndex)
        {
            if (importedIndex < 0)
            {
                return InvalidTextureIndex;
            }

            if (importedIndex >=
                static_cast<int32_t>(
                    textures.importedToCompiledTexture.size()))
            {
                return InvalidTextureIndex;
            }

            const int32_t compiled =
                textures.importedToCompiledTexture[
                    importedIndex];

            if (compiled < 0)
            {
                return InvalidTextureIndex;
            }

            return static_cast<uint32_t>(
                compiled);
        }

        uint32_t buildFlags(
            const ImportedMaterial& material,
            const SceneTextureCompilerResult& textures)
        {
            MaterialFlags flags =
                MaterialFlags::None;

            auto hasTexture = [&textures](int32_t importedTexture)
                {
                    if (importedTexture < 0)
                    {
                        return false;
                    }

                    if (importedTexture >=
                        static_cast<int32_t>(
                            textures.importedToCompiledTexture.size()))
                    {
                        return false;
                    }

                    return
                        textures.importedToCompiledTexture[
                            importedTexture] >= 0;
                };

            if (hasTexture(material.albedoTexture))
            {
                flags |= MaterialFlags::HasAlbedoMap;
            }

            if (hasTexture(material.normalTexture))
            {
                flags |= MaterialFlags::HasNormalMap;
            }

            if (hasTexture(material.ormTexture))
            {
                flags |= MaterialFlags::HasORMMap;
            }

            if (hasTexture(material.emissiveTexture))
            {
                flags |= MaterialFlags::HasEmissiveMap;
            }

            if (material.doubleSided)
            {
                flags |= MaterialFlags::DoubleSided;
            }

            switch (material.alphaMode)
            {
                case ImportedAlphaMode::Mask:
                    flags |= MaterialFlags::AlphaMask;
                    break;

                case ImportedAlphaMode::Blend:
                    flags |= MaterialFlags::AlphaBlend;
                    break;

                default:
                    break;
            }

            const bool emissiveFactor =
                material.emissiveFactor.r > 0.0f ||
                material.emissiveFactor.g > 0.0f ||
                material.emissiveFactor.b > 0.0f;

            if (emissiveFactor ||
                hasTexture(material.emissiveTexture))
            {
                flags |= MaterialFlags::Emissive;
            }

            return static_cast<uint32_t>(
                flags);
        }

        AlphaMode convertAlphaMode(
            ImportedAlphaMode mode)
        {
            switch (mode)
            {
                case ImportedAlphaMode::Mask:
                    return AlphaMode::Mask;

                case ImportedAlphaMode::Blend:
                    return AlphaMode::Blend;

                case ImportedAlphaMode::Opaque:
                default:
                    return AlphaMode::Opaque;
            }
        }
    }

    MaterialBuildResult MaterialBuilder::build(
        const ImportedScene& scene,
        const SceneTextureCompilerResult& textures)
    {
        MaterialBuildResult result{};

        result.materials.reserve(
            scene.materials.size());

        for (const ImportedMaterial& imported :
             scene.materials)
        {
            MaterialInfo material{};

            material.baseColorFactor =
                imported.baseColorFactor;

            material.emissiveFactor =
                imported.emissiveFactor;

            material.metallicFactor =
                imported.metallicFactor;

            material.roughnessFactor =
                imported.roughnessFactor;

            material.occlusionStrength =
                imported.occlusionStrength;

            material.emissiveStrength =
                imported.emissiveStrength;

            material.alphaCutoff =
                imported.alphaCutoff;

            material.alphaMode =
                convertAlphaMode(
                    imported.alphaMode);

            material.flags =
                buildFlags(
                    imported,
                    textures);

            material.albedoTexture =
                resolveTexture(
                    textures,
                    imported.albedoTexture);

            material.normalTexture =
                resolveTexture(
                    textures,
                    imported.normalTexture);

            material.ormTexture =
                resolveTexture(
                    textures,
                    imported.ormTexture);

            material.occlusionTexture =
                resolveTexture(
                    textures,
                    imported.occlusionTexture);

            material.roughnessTexture =
                resolveTexture(
                    textures,
                    imported.roughnessTexture);

            material.metallicTexture =
                resolveTexture(
                    textures,
                    imported.metallicTexture);

            material.emissiveTexture =
                resolveTexture(
                    textures,
                    imported.emissiveTexture);

            result.materials.push_back(
                std::move(material));
        }

        return result;
    }
}
