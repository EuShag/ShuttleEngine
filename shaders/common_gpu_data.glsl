#ifndef COMMON_GPU_DATA_GLSL
#define COMMON_GPU_DATA_GLSL

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

#define INVALID_DESCRIPTOR_U32 0xffffffffu
#define INVALID_INDEX_U32 0xffffffffu

// ============================================================
// Render root
// ============================================================

struct RenderRootData
{
    uint64_t commonDataDeviceAddress;
    uint64_t sceneDataDeviceAddress;
    uint64_t environmentDataDeviceAddress;
    uint64_t cameraDataDeviceAddress;
};

// ============================================================
// Common resources
// ============================================================

struct CommonResourcesInfo
{
    uint brdfLutTexture;

    uint materialSampler;
    uint shadowSampler;
    uint nearestSampler;
};

// ============================================================
// Camera
// ============================================================

struct CameraData
{
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;

    mat4 inverseViewMatrix;
    mat4 inverseProjectionMatrix;
    mat4 inverseViewProjectionMatrix;

    vec4 cameraPosition;

    float nearPlane;
    float farPlane;
    float fov;
    float aspectRatio;
};

// ============================================================
// Scene root
// ============================================================

struct SceneGpuInfo
{
    uint64_t sceneNodesBufferDeviceAddress;
    uint64_t materialDatasBufferAddress;
    uint64_t lightDatasBufferAddress;
    uint64_t meshDatasBufferAddress;
    uint64_t drawablesBufferAddress;
    uint64_t localTransformsBufferAddress;

    uint materialCount;
    uint lightCount;
    uint meshCount;
    uint drawableCount;
    uint nodeCount;
    uint padding0;
};

// ============================================================
// Environment
// ============================================================

struct EnvironmentGpuInfo
{
    uint skyboxTexture;
    uint irradianceTexture;
    uint radianceTexture;
    uint reserved;
};

// ============================================================
// Scene graph
// ============================================================

struct SceneNode
{
    uint parentIndex;
    uint transformIndex;
    uint animationBindingIndex;
    uint nodeNameHash;
};

struct LocalTransformGpuInfo
{
    vec4 translation;
    vec4 rotation;
    vec4 scale;
};

struct DrawableGpuInfo
{
    uint meshIndex;
    uint materialIndex;
    uint transformIndex;
    uint reserved;
};

// ============================================================
// Geometry
// ============================================================

struct MeshLodGpuInfo
{
    uint firstIndex;
    uint indexCount;

    float geometricError;
    float screenThreshold;
};

struct MeshGpuInfo
{
    uint64_t positionAttributeBufferAddress;
    uint64_t normalUvTangentAttributeBufferAddress;

    vec4 boundingSphere;
    vec4 minAABB;
    vec4 maxAABB;

    MeshLodGpuInfo lods[4];

    uint lodCount;
    uint meshFlags;
    uint reserved0;
    uint reserved1;
};

// ============================================================
// Material
// ============================================================

struct MaterialGpuInfo
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;

    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float occlusionStrength;

    float emissiveStrength;

    uint albedoTexture;
    uint normalTexture;
    uint ormTexture;
    uint emissiveTexture;

    uint flags;
    uint pipelineFlags;
    uint alphaMode;
};

// ============================================================
// Lighting
// ============================================================

struct DirectionalLightGpuInfo
{
    vec4 lightDirection;
    vec4 lightColorAndIntensity;
};

// ============================================================
// Buffer references
// ============================================================

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer RenderRootDataRef
{
    RenderRootData value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CommonResourcesInfoRef
{
    CommonResourcesInfo value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CameraDataRef
{
    CameraData value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer SceneGpuInfoRef
{
    SceneGpuInfo value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer EnvironmentGpuInfoRef
{
    EnvironmentGpuInfo value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer SceneNodeBufferRef
{
    SceneNode values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer LocalTransformGpuInfoRef
{
    LocalTransformGpuInfo values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer DrawableGpuInfoRef
{
    DrawableGpuInfo values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer MeshGpuInfoRef
{
    MeshGpuInfo values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer MaterialGpuInfoRef
{
    MaterialGpuInfo values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CameraDataRef
{
    CameraData value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CommonResourcesInfoRef
{
    CommonResourcesInfo value;
};