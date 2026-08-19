#include "SceneDataLoader.hpp"

namespace shuttle::engine::render
{
    namespace
    {
        template <typename T> vk::ResultValue<std::span<const T>> readTypedSection(
            const assets::core::BlobView& blob,
            assets::core::BlobSectionType type)
        {
            const std::optional<assets::core::BlobSection> section = blob.findSection(type);

            if (!section) return { vk::Result::eSuccess, {} };
            const std::span<const uint8_t> bytes = blob.bytes(*section);
            if (bytes.empty()) return { vk::Result::eSuccess, {} };
            if (bytes.size_bytes() % sizeof(T) != 0) return { vk::Result::eErrorInitializationFailed, {} };

            return {
                vk::Result::eSuccess,
                std::span<const T>(reinterpret_cast<const T*>(bytes.data()), bytes.size_bytes() / sizeof(T))
            };
        }

        vk::ResultValue<std::span<const uint8_t>>
        readRawSection(
            const assets::core::BlobView& blob,
            assets::core::BlobSectionType type)
        {
            const std::optional<assets::core::BlobSection> section = blob.findSection(type);

            if (!section) return { vk::Result::eErrorInitializationFailed, {} };

            return { vk::Result::eSuccess, blob.bytes(*section) };
        }
    }

    vk::ResultValue<LoadedSceneData> loadSceneData(std::filesystem::path const& path)
    {
        LoadedSceneData result{};

        auto blob = assets::core::BlobReader::open(path);

        auto [readNodesResult, nodes] =
            readTypedSection<assets::formats::scene::SceneNode>(blob, assets::core::BlobSectionType::GpuSceneNodes);

        if (readNodesResult != vk::Result::eSuccess) return {readNodesResult, {}};
        result.nodes = nodes;

        auto [readLevelsResult, levels] =
            readTypedSection<assets::formats::scene::NodeLevelRange>(blob, assets::core::BlobSectionType::GpuNodeLevels);

        if (readLevelsResult != vk::Result::eSuccess) return {readLevelsResult, {}};
        result.levels = levels;

        auto [readTransformsResult, transforms] =
            readTypedSection<assets::formats::scene::Transform>(blob, assets::core::BlobSectionType::GpuSceneTransforms);

        if (readTransformsResult != vk::Result::eSuccess) return {readTransformsResult, {}};
        result.transforms = transforms;

        auto [readDrawablesResult, drawables] =
            readTypedSection<assets::formats::scene::GpuDrawableObject>(blob, assets::core::BlobSectionType::GpuDrawableObjects);

        if (readDrawablesResult != vk::Result::eSuccess) return {readDrawablesResult, {}};
        result.drawables = drawables;

        auto [readDirectionalLightsResult, directionalLights] =
            readTypedSection<assets::formats::lighting::DirectionalLight>(blob, assets::core::BlobSectionType::GpuDirectionalLights);

        if (readDirectionalLightsResult != vk::Result::eSuccess){
            return {readDirectionalLightsResult, {} };
        }

        result.directionalLights = directionalLights;

        auto [readPositionsResult, positions] =
            readTypedSection<assets::formats::PositionAttribute>(blob, assets::core::BlobSectionType::PositionMegabuffer);

        if (readPositionsResult != vk::Result::eSuccess) return {readPositionsResult, {}};
        result.positions = positions;

        auto [readAttributesResult, attributes] =
            readTypedSection<assets::formats::VertexAttribute>(blob, assets::core::BlobSectionType::AttributeMegabuffer);

        if (readAttributesResult != vk::Result::eSuccess) return {readAttributesResult, {}};
        result.attributes = attributes;

        auto [readIndicesResult, indices] =
            readTypedSection<uint32_t>(blob, assets::core::BlobSectionType::IndexMegabuffer);

        if (readIndicesResult != vk::Result::eSuccess) return {readIndicesResult, {}};

        result.indices = indices;

        auto [readMeshesResult, meshes] =
            readTypedSection<assets::formats::geometry::GpuMesh>(blob, assets::core::BlobSectionType::GpuMeshes);

        if (readMeshesResult != vk::Result::eSuccess) return {readMeshesResult, {}};
        result.meshes = meshes;

        auto [readMaterialsResult, materials] =
            readTypedSection<assets::formats::material::MaterialInfo>(blob, assets::core::BlobSectionType::GpuMaterials);

        if (readMaterialsResult != vk::Result::eSuccess) return {readMaterialsResult, {}};
        result.materials = materials;

        auto [readTextureMetadataResult,
              textureMetadatas] = readTypedSection<assets::formats::texture::TextureMetadata>(
                  blob, assets::core::BlobSectionType::TextureMetadata);

        if (readTextureMetadataResult != vk::Result::eSuccess) return { readTextureMetadataResult,{} };
        result.textureMetadatas = textureMetadatas;

        auto [readTextureMipMetadataResult,
              textureMipMetadatas] = readTypedSection<assets::formats::texture::TextureMipMetadata>(
                    blob, assets::core::BlobSectionType::TextureMipMetadata);

        if (readTextureMipMetadataResult != vk::Result::eSuccess) return {readTextureMipMetadataResult,{}};
        result.textureMipMetadatas = textureMipMetadatas;

        auto [readTextureBytesResult, textureBytes] =
            readRawSection(blob, assets::core::BlobSectionType::TextureData);

        if (readTextureBytesResult != vk::Result::eSuccess) return {readTextureBytesResult, {} };
        result.textureBytes = textureBytes;


        result.sceneBlobView = std::move(blob);

        return {vk::Result::eSuccess, std::move(result)};
    }
}