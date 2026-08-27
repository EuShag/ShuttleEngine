#include "CompiledSceneBlobWriter.hpp"

#include <Assets/Core/BlobWriter.hpp>
#include <Assets/Formats/Texture.hpp>
#include <Assets/TextureCompiler/CompiledTexture.hpp>

#include <span>
#include <vector>

namespace shuttle::assets::scene_compiler
{
namespace
{
using namespace shuttle::assets;

template <typename T>
void addTypedSection(core::BlobWriter& writer, core::BlobSectionType type, const std::vector<T>& data)
{
    if (data.empty())
    {
        return;
    }

    writer.addTypedSection(type, std::span<const T>(data.data(), data.size()));
}

struct TextureSerializationData
{
    std::vector<formats::texture::TextureMetadata> metadata;

    std::vector<formats::texture::TextureMipMetadata> mipMetadata;

    std::vector<uint8_t> textureData;
};

TextureSerializationData buildTextureData(const CompiledScene& scene)
{
    TextureSerializationData result;

    uint64_t globalDataOffset = 0;
    uint64_t globalMipOffset = 0;

    result.metadata.reserve(scene.textures.size());

    for (const auto& texture : scene.textures)
    {
        auto metadata = texture.metadata;

        metadata.mipTableOffset = globalMipOffset;

        result.metadata.push_back(metadata);

        for (auto mip : texture.mipMetadata)
        {
            mip.dataOffset += globalDataOffset;

            result.mipMetadata.push_back(mip);
        }

        globalMipOffset +=
            texture.mipMetadata.size() * sizeof(formats::texture::TextureMipMetadata);

        result.textureData.insert(result.textureData.end(), texture.data.begin(), texture.data.end());

        globalDataOffset += texture.data.size();
    }

    return result;
}
} // namespace

bool CompiledSceneBlobWriter::write(const CompiledScene& scene, const std::filesystem::path& outputPath)
{
    core::BlobWriter writer;

    //
    // textures
    //

    {
        TextureSerializationData textures = buildTextureData(scene);

        addTypedSection(writer, core::BlobSectionType::TextureMetadata, textures.metadata);

        addTypedSection(writer, core::BlobSectionType::TextureMipMetadata, textures.mipMetadata);

        if (!textures.textureData.empty())
        {
            writer.addSection(core::BlobSectionType::TextureData,
                              std::span<const uint8_t>(textures.textureData.data(), textures.textureData.size()));
        }
    }

    //
    // materials
    //

    addTypedSection(writer, core::BlobSectionType::GpuMaterials, scene.materials);

    //
    // geometry
    //

    addTypedSection(writer, core::BlobSectionType::PositionMegabuffer, scene.positions);

    addTypedSection(writer, core::BlobSectionType::AttributeMegabuffer, scene.attributes);

    addTypedSection(writer, core::BlobSectionType::SkinMegabuffer, scene.skins);

    addTypedSection(writer, core::BlobSectionType::IndexMegabuffer, scene.indices);

    addTypedSection(writer, core::BlobSectionType::GpuMeshes, scene.meshes);

    //
    // scene graph
    //

    addTypedSection(writer, core::BlobSectionType::GpuSceneNodes, scene.nodes);

    addTypedSection(writer, core::BlobSectionType::GpuNodeLevels, scene.levels);

    addTypedSection(writer, core::BlobSectionType::GpuDrawableObjects, scene.drawableObjects);

    addTypedSection(writer, core::BlobSectionType::GpuSceneTransforms, scene.transforms);

    //
    // lighting
    //

    addTypedSection(writer, core::BlobSectionType::GpuDirectionalLights, scene.directionalLights);

    addTypedSection(writer, core::BlobSectionType::GpuPointLights, scene.pointLights);

    addTypedSection(writer, core::BlobSectionType::GpuSpotLights, scene.spotLights);

    return writer.write(outputPath);
}
} // namespace shuttle::assets::scene_compiler