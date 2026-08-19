#pragma once

#include "Assets/Core/BlobReader.hpp"
#include "Assets/Formats/Geometry.hpp"
#include "Assets/Formats/Material.hpp"
#include "Assets/Formats/Texture.hpp"

#include "IncludeVulkan.hpp"

#include <span>

#include "Assets/Formats/Lighting.hpp"
#include "Assets/Formats/Scene.hpp"

namespace shuttle::engine::render
{
    struct LoadedSceneData
    {
        std::span<const assets::formats::scene::SceneNode> nodes;
        std::span<const assets::formats::scene::NodeLevelRange> levels;
        std::span<const assets::formats::scene::Transform> transforms;
        std::span<const assets::formats::scene::GpuDrawableObject> drawables;
        std::span<const assets::formats::lighting::DirectionalLight> directionalLights;
        std::span<const assets::formats::PositionAttribute> positions;
        std::span<const assets::formats::VertexAttribute> attributes;
        std::span<const uint32_t> indices;
        std::span<const assets::formats::geometry::GpuMesh> meshes;
        std::span<const assets::formats::material::MaterialInfo> materials;
        std::span<const assets::formats::texture::TextureMetadata> textureMetadatas;
        std::span<const assets::formats::texture::TextureMipMetadata> textureMipMetadatas;
        std::span<const uint8_t> textureBytes;

        assets::core::BlobView sceneBlobView;
    };

    [[nodiscard]] vk::ResultValue<LoadedSceneData> loadSceneData(std::filesystem::path const& path);
}
