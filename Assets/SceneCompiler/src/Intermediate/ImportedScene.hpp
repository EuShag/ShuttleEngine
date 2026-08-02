#pragma once

#include <Assets/Formats/Texture.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace shuttle::assets::scene_compiler
{
inline constexpr int32_t InvalidIndexI32 = -1;
inline constexpr uint32_t InvalidIndexU32 = 0xFFFFFFFFu;

enum class ImportedTextureSourceKind : uint32_t
{
    None,
    File,
    EmbeddedEncoded,
    EmbeddedRGBA,
    Generated
};

struct ImportedTexture
{
    std::string name;

    ImportedTextureSourceKind sourceKind = ImportedTextureSourceKind::None;

    std::filesystem::path sourcePath;

    std::vector<uint8_t> embeddedBytes;

    std::string formatHint;

    uint32_t width = 0;
    uint32_t height = 0;

    formats::texture::TextureSemantic semantic = formats::texture::TextureSemantic::Unknown;
};

enum class ImportedAlphaMode : uint32_t
{
    Opaque,
    Mask,
    Blend
};

struct ImportedMaterial
{
    std::string name;

    glm::vec4 baseColorFactor{1.0f};
    glm::vec4 emissiveFactor{0.0f};

    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float occlusionStrength = 1.0f;
    float emissiveStrength = 1.0f;
    float alphaCutoff = 0.5f;

    ImportedAlphaMode alphaMode = ImportedAlphaMode::Opaque;

    bool doubleSided = false;

    int32_t albedoTexture = InvalidIndexI32;
    int32_t normalTexture = InvalidIndexI32;
    int32_t ormTexture = InvalidIndexI32;
    int32_t occlusionTexture = InvalidIndexI32;
    int32_t roughnessTexture = InvalidIndexI32;
    int32_t metallicTexture = InvalidIndexI32;
    int32_t emissiveTexture = InvalidIndexI32;
};

struct ImportedVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec4 tangent{0.0f};
    glm::vec2 texCoord0{0.0f};
    glm::vec4 color{1.0f};
};

struct VertexBoneInfluence
{
    std::array<uint16_t, 4> boneIndices{};
    std::array<float, 4> boneWeights{};
};

struct ImportedMorphVertexDelta
{
    uint32_t vertexIndex = 0;

    glm::vec3 positionDelta{0.0f};
    glm::vec3 normalDelta{0.0f};
    glm::vec3 tangentDelta{0.0f};
};

struct ImportedMorphTarget
{
    std::string name;

    std::vector<ImportedMorphVertexDelta> deltas;

    glm::vec3 maxPositionDelta{0.0f};
};

struct ImportedMesh
{
    std::string name;

    std::vector<ImportedVertex> vertices;
    std::vector<uint32_t> indices;

    std::vector<VertexBoneInfluence> skinning;

    std::vector<ImportedMorphTarget> morphTargets;

    int32_t materialIndex = InvalidIndexI32;
    int32_t skinIndex = InvalidIndexI32;
};

struct ImportedBone
{
    std::string name;

    uint32_t nodeIndex = InvalidIndexU32;
    uint32_t parentBone = InvalidIndexU32;

    glm::mat4 inverseBindMatrix{1.0f};
};

struct ImportedSkin
{
    std::string name;

    uint32_t skeletonRoot = InvalidIndexU32;

    std::vector<ImportedBone> bones;
};

enum class ImportedInterpolationMode : uint8_t
{
    Step,
    Linear,
    CubicSpline
};

template <typename TValue> struct AnimationKey
{
    double time = 0.0;

    TValue value{};

    TValue inTangent{};
    TValue outTangent{};
};

using PositionKey = AnimationKey<glm::vec3>;
using RotationKey = AnimationKey<glm::quat>;
using ScaleKey = AnimationKey<glm::vec3>;
using WeightKey = AnimationKey<std::vector<float>>;

struct ImportedAnimationChannel
{
    uint32_t nodeIndex = InvalidIndexU32;

    ImportedInterpolationMode translationInterpolation = ImportedInterpolationMode::Linear;

    ImportedInterpolationMode rotationInterpolation = ImportedInterpolationMode::Linear;

    ImportedInterpolationMode scaleInterpolation = ImportedInterpolationMode::Linear;

    ImportedInterpolationMode weightInterpolation = ImportedInterpolationMode::Linear;

    std::vector<PositionKey> positions;
    std::vector<RotationKey> rotations;
    std::vector<ScaleKey> scales;
    std::vector<WeightKey> weights;
};

struct ImportedAnimationEvent
{
    std::string name;

    double time = 0.0;
};

struct RootMotionTrack
{
    std::vector<PositionKey> positions;
    std::vector<RotationKey> rotations;
};

struct ImportedAnimation
{
    std::string name;

    double startTime = 0.0;
    double endTime = 0.0;
    double duration = 0.0;
    double ticksPerSecond = 0.0;

    bool looping = true;

    RootMotionTrack rootMotion;

    std::vector<ImportedAnimationChannel> channels;
    std::vector<ImportedAnimationEvent> events;
};

enum class ImportedLightType : uint32_t
{
    Directional,
    Point,
    Spot
};

struct ImportedLight
{
    std::string name;

    ImportedLightType type = ImportedLightType::Point;

    glm::vec3 color{1.0f};

    float intensity = 1.0f;
    float range = 0.0f;

    float innerConeAngle = 0.0f;
    float outerConeAngle = 0.0f;
};

struct ImportedNode
{
    std::string name;

    glm::mat4 localTransform{1.0f};

    int32_t parent = InvalidIndexI32;

    std::vector<int32_t> children;
    std::vector<int32_t> meshes;

    int32_t lightIndex = InvalidIndexI32;
    int32_t skinIndex = InvalidIndexI32;
};

struct ImportedDirectionalLight
{
    glm::vec3 direction;
    glm::vec3 color;

    float intensity;
    bool castShadows;
};

struct ImportedPointLight
{
    glm::vec3 position;
    glm::vec3 color;

    float intensity;
    float range;
};

struct ImportedSpotLight
{
    glm::vec3 position;
    glm::vec3 direction;

    glm::vec3 color;

    float intensity;

    float innerCone;
    float outerCone;
};

struct ImportedScene
{
    std::vector<ImportedNode> nodes;

    std::vector<ImportedMesh> meshes;

    std::vector<ImportedMaterial> materials;

    std::vector<ImportedTexture> textures;

    std::vector<ImportedLight> lights;

    std::vector<ImportedSkin> skins;

    std::vector<ImportedAnimation> animations;

    std::vector<ImportedDirectionalLight> directionalLights;

    std::vector<ImportedPointLight> pointLights;

    std::vector<ImportedSpotLight> spotLights;
};
} // namespace shuttle::assets::scene_compiler