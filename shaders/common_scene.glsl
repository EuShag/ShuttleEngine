#ifndef SHUTTLE_COMMON_SCENE_GLSL
#define SHUTTLE_COMMON_SCENE_GLSL

// ============================================================
// Constants
// ============================================================

#define MAX_MESH_LODS 4u

#define INVALID_INDEX_U32 0xFFFFFFFFu
#define INVALID_TEXTURE_U32 0xFFFFFFFFu

// ============================================================
// Material flags
// ============================================================

#define MATERIAL_FLAG_HAS_ALBEDO_MAP (1u << 0)
#define MATERIAL_FLAG_HAS_NORMAL_MAP (1u << 1)
#define MATERIAL_FLAG_HAS_ORM_MAP (1u << 2)
#define MATERIAL_FLAG_HAS_EMISSIVE_MAP (1u << 3)

#define MATERIAL_FLAG_ALPHA_MASK (1u << 4)
#define MATERIAL_FLAG_ALPHA_BLEND (1u << 5)

#define MATERIAL_FLAG_DOUBLE_SIDED (1u << 6)

#define MATERIAL_FLAG_EMISSIVE (1u << 7)

// ============================================================
// Alpha mode
// ============================================================

#define ALPHA_MODE_OPAQUE 0u
#define ALPHA_MODE_MASK 1u
#define ALPHA_MODE_BLEND 2u

// ============================================================
// Mesh flags
// ============================================================

#define MESH_FLAG_HAS_SKINNING (1u << 0)
#define MESH_FLAG_HAS_MORPHING (1u << 1)

#define MESH_FLAG_OCCLUDER (1u << 2)
#define MESH_FLAG_TRANSPARENT (1u << 3)
#define MESH_FLAG_DOUBLE_SIDED (1u << 4)
#define MESH_FLAG_SHADOW_CASTER (1u << 5)

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

struct NodeLevelRange
{
    uint startNodeIndex;
    uint nodeCount;

    uint reserved0;
    uint reserved1;
};

struct LocalTransform
{
    vec3 translation;
    uint reserved0;

    vec4 rotationQuat;

    vec3 scale;
    uint reserved1;
};

// ============================================================
// Geometry
// ============================================================

struct PositionAttribute
{
    vec4 position;
};

struct VertexAttribute
{
    vec4 normal;
    vec4 tangent;
    vec4 uv;
};

struct MeshLod
{
    uint firstIndex;
    uint indexCount;

    float geometricError;
    float screenThreshold;
};

struct GpuMesh
{
    uint positionOffset;
    uint attributeOffset;

    uint lodCount;
    uint meshFlags;

    vec4 boundingSphere;
    // xyz = center
    // w   = radius

    vec4 minBounds;
    vec4 maxBounds;

    MeshLod lods[MAX_MESH_LODS];
};

// ============================================================
// Drawable
// ============================================================

struct GpuDrawableObject
{
    uint transformIndex;
    uint meshIndex;
    uint materialIndex;
    uint flags;
};

// ============================================================
// Material
// ============================================================

struct MaterialInfo
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;

    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float occlusionStrength;

    float emissiveStrength;

    uint flags;
    uint pipelineFlags;
    uint alphaMode;

    uint albedoTexture;
    uint normalTexture;
    uint ormTexture;
    uint emissiveTexture;

    uint reserved0;
};

// ============================================================
// Lighting
// ============================================================

struct DirectionalLightData
{
    vec4 directionAndIntensity;
    // xyz = direction
    // w   = intensity

    vec3 color;
    uint castShadows;
};

struct PointLightData
{
    vec4 positionAndRadius;
    // xyz = position
    // w   = radius

    vec3 color;
    float intensity;
};

struct SpotLightData
{
    vec4 positionAndRadius;
    vec4 directionAndIntensity;

    vec3 color;
    float innerCutoffCos;

    float outerCutoffCos;
    uint castShadows;

    uint reserved0;
    uint reserved1;
};

// ============================================================
// Scene info
// ============================================================

struct SceneInfo
{
    uint drawableObjectCount;
    uint transformCount;

    uint directionalLightCount;
    uint directionalShadowCasterCount;
    uint materialCount;
    uint textureCount;

    uint reserved0;
    uint reserved1;
};

// ============================================================
// Vulkan indirect command
// ============================================================

struct VkDrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

#endif